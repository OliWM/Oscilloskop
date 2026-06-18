#include <avr/io.h>
#include <avr/interrupt.h>
#include "ADC.h"
#include "timer.h"

#define BAUD 115200

// Double buffers
volatile uint8_t buffer_A[MAX_RECORD_LENGTH];
volatile uint8_t buffer_B[MAX_RECORD_LENGTH];
volatile uint8_t *active_buffer  = buffer_A;
volatile uint8_t *ready_buffer   = buffer_B;
volatile uint8_t  buffer_ready   = 0;
volatile uint16_t buffer_index   = 0;
volatile uint16_t record_length  = MAX_RECORD_LENGTH;
volatile uint16_t current_sample_rate = 0;

// Klem værdier modtaget fra UART ind i sikre grænser, FØR de bruges.
// record_length styrer hvornår ADC_vect skifter buffer — en uklemt værdi
// over MAX_RECORD_LENGTH får ISR'en til at skrive uden for buffer_A/buffer_B.
static uint16_t clamp_sample_rate(uint16_t sr)
{
    if (sr < 10) return 10;
    if (sr > 10000) return 10000;
    return sr;
}

static uint16_t clamp_record_length(uint16_t rl, uint16_t sr)
{
    uint32_t byte_rate = (uint32_t)BAUD / 10;
    if (sr >= byte_rate) // lidt unødvendigt. Men HVIS vi skulle ændre til en lavere BAUD end 115200 eller højere sr end 10.000 så kunne vi få en bug uden.
    {
        return 0; //så ved vi da der er sket en fejl. 
    }

    uint32_t datapak = (uint32_t)7 * sr; //parentesen fordi sr kun er uint16, så vi skal bruge at 7 er uint32 for ikk at risikere at den wrapper

    if (rl <= (datapak / (byte_rate - sr)))
    {
        if ((datapak % (byte_rate - sr)) == 0){
            rl = (datapak / (byte_rate - sr)); 
        }
        else 
        {
            rl = (datapak / (byte_rate - sr)) + 1;
        }
    }

    rl = rl * 1.1;

    if (rl > MAX_RECORD_LENGTH) return MAX_RECORD_LENGTH;

    return rl;
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
    sample_rate = clamp_sample_rate(sample_rate);
    rec_length  = clamp_record_length(rec_length, sample_rate);

    current_sample_rate = sample_rate;
    record_length = rec_length;

    // REFS1:0 = 00 → ekstern AREF
    // ADLAR   = 1  → venstrejustering (læs 8-bit fra ADCH)
    // MUX     = 0  → ADC0 (juster pin efter behov)
    ADMUX = (0 << REFS1) | (0 << REFS0) | (1 << ADLAR) | (0 << MUX0);

    // Prescaler 16 → ADC clock = 16 MHz / 16 = 1 MHz (~76.900 SPS max)
    // Over 200 kHz mister ADC'en 10-bit nøjagtighed, men vi bruger kun
    // de øverste 8 bit (ADLAR=1 → ADCH), så det er acceptabelt.
    // ADEN  = ADC enable
    // ADIE  = ADC interrupt enable
    ADCSRA = (1 << ADEN) | (1 << ADIE)
           | (1 << ADPS2) | (0 << ADPS1) | (0 << ADPS0); // prescaler 16

    timer_init(sample_rate);

    sei();
}

// ---------------------------------------------------------------------------
// Opdater parametre under kørsel (kaldes fra UART RX handler)
// ---------------------------------------------------------------------------
void adc_set_params(uint16_t sample_rate, uint16_t rec_length)
{
    sample_rate = clamp_sample_rate(sample_rate);
    rec_length  = clamp_record_length(rec_length, sample_rate);

    cli();
    current_sample_rate = sample_rate;
    record_length = rec_length;
    buffer_index  = 0;
    buffer_ready  = 0;
    timer_set_samplerate(sample_rate);
    sei();
}