#include <avr/io.h>
#include <avr/interrupt.h>
#include "timer.h"

// ---------------------------------------------------------------------------
// Timer1 CTC ISR — trigger ADC konvertering
// ---------------------------------------------------------------------------
ISR(TIMER1_COMPA_vect)
{
    // Start ADC single conversion
    ADCSRA |= (1 << ADSC);
}

// ---------------------------------------------------------------------------
// Initialisering
// ---------------------------------------------------------------------------
void timer_init(uint16_t sample_rate)
{
    // WGM12 = CTC mode (OCR1A som top)
    // CS11 + CS10 = prescaler 64 → Timer clock = 16 MHz / 64 = 250 kHz
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);

    // OCR1A = (Timer_clock / sample_rate) - 1
    OCR1A = (TIMER_CLOCK_HZ / sample_rate) - 1;

    // Aktiver Timer1 Compare Match A interrupt
    TIMSK1 = (1 << OCIE1A);
}

// ---------------------------------------------------------------------------
// Opdater samplerate under kørsel
// ---------------------------------------------------------------------------
void timer_set_samplerate(uint16_t sample_rate)
{
    OCR1A = (TIMER_CLOCK_HZ / sample_rate) - 1;
}