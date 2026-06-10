#include <avr/io.h>
#include <avr/interrupt.h>
#include "ADC.h"

// Double buffers
volatile uint8_t buffer_A[MAX_RECORD_LENGTH];
volatile uint8_t buffer_B[MAX_RECORD_LENGTH];
volatile uint8_t *active_buffer  = buffer_A;
volatile uint8_t *ready_buffer   = buffer_B;
volatile uint8_t  buffer_ready   = 0;
volatile uint16_t buffer_index   = 0;
volatile uint16_t record_length  = BUFFER_SIZE;

// ---------------------------------------------------------------------------
// Timer1 CTC ISR — trigger ADC konvertering
// ---------------------------------------------------------------------------
ISR(TIMER1_COMPA_vect)
{
    // Start ADC single conversion
    ADCSRA |= (1 << ADSC);
}

// ---------------------------------------------------------------------------
// ADC Complete ISR — læs sample, håndter buffer-skift
// ---------------------------------------------------------------------------
ISR(ADC_vect)
{
    // ADLAR=1: ADCH indeholder de 8 MSB (vores 8-bit sample)
    active_buffer[buffer_index++] = ADCH;

    if (buffer_index >= record_length) {
        buffer_index = 0;

        if (!buffer_ready) {
            // Skift buffer (dobbelt-buffering)
            volatile uint8_t *tmp = active_buffer;
            active_buffer         = ready_buffer;
            ready_buffer          = tmp;
            buffer_ready          = 1;   // Signalér main loop
        }
        // Hvis buffer_ready stadig er sat er forrige pakke ikke sendt endnu —
        // vi overwriter active_buffer (sample-drop, men ingen korruption)
    }
}

// ---------------------------------------------------------------------------
// Initialisering
// ---------------------------------------------------------------------------
void adc_init(uint16_t sample_rate, uint16_t rec_length)
{
    record_length = rec_length;

    // --- ADC ---
    // REFS1:0 = 00 → ekstern AREF
    // ADLAR   = 1  → venstrejustering (læs 8-bit fra ADCH)
    // MUX     = 0  → ADC0 (juster pin efter behov)
    ADMUX = (0 << REFS1) | (0 << REFS0) | (1 << ADLAR) | (0 << MUX0);

    // Prescaler 64 → ADC clock = 16 MHz / 64 = 250 kHz (~19.200 SPS max)
    // ADEN  = ADC enable
    // ADIE  = ADC interrupt enable
    ADCSRA = (1 << ADEN) | (1 << ADIE)
           | (1 << ADPS2) | (1 << ADPS1) | (0 << ADPS0); // prescaler 64

    // --- Timer1 CTC ---
    // WGM12 = CTC mode (OCR1A som top)
    // CS11 + CS10 = prescaler 64 → Timer clock = 16 MHz / 64 = 250 kHz
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);

    // OCR1A bestemmer samplerate:
    // OCR1A = (Timer_clock / sample_rate) - 1
    OCR1A = (250000UL / sample_rate) - 1;

    // Aktiver Timer1 Compare Match A interrupt
    TIMSK1 = (1 << OCIE1A);

    sei();
}

// ---------------------------------------------------------------------------
// Opdater parametre under kørsel (kaldes fra UART RX handler)
// ---------------------------------------------------------------------------
void adc_set_params(uint16_t sample_rate, uint16_t rec_length)
{
    cli();
    record_length = rec_length;
    buffer_index  = 0;
    buffer_ready  = 0;
    OCR1A = (250000UL / sample_rate) - 1;
    sei();
}