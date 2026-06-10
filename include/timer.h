#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// Timer clock = 16 MHz / 64 = 250 kHz
#define TIMER_CLOCK_HZ 250000UL

// Initialiserer Timer1 i CTC-mode med given samplerate
// sample_rate: ønsket samplerate i SPS (10–10000)
void timer_init(uint16_t sample_rate);

// Opdaterer samplerate under kørsel
void timer_set_samplerate(uint16_t sample_rate);

#endif // TIMER_H