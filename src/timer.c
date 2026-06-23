#include <avr/io.h>
#include <avr/interrupt.h>
#include "timer.h"

ISR(TIMER1_COMPA_vect)
{
    ADCSRA |= (1 << ADSC);
}

void timer_init(uint16_t sample_rate)
{
    // CTC mode, prescaler 64
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);

    OCR1A = (TIMER_CLOCK_HZ / sample_rate) - 1;

    TIMSK1 = (1 << OCIE1A);
}

void timer_set_samplerate(uint16_t sample_rate)
{
    OCR1A = (TIMER_CLOCK_HZ / sample_rate) - 1;
}