#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

#define TIMER_CLOCK_HZ 250000UL // 16 MHz / 64

void timer_init(uint16_t sample_rate);
void timer_set_samplerate(uint16_t sample_rate);

#endif // TIMER_H