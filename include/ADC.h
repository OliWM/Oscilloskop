#ifndef ADC_H
#define ADC_H

#include <stdint.h>

#define MAX_RECORD_LENGTH 1000

extern volatile uint8_t buffer_A[MAX_RECORD_LENGTH];
extern volatile uint8_t buffer_B[MAX_RECORD_LENGTH];
extern volatile uint8_t *active_buffer;    // ISR skriver hertil
extern volatile uint8_t *ready_buffer;     // Klar til afsendelse
extern volatile uint8_t  buffer_ready;     // Flag: klar buffer venter på afsendelse
extern volatile uint16_t buffer_index;
extern volatile uint16_t record_length;    // Antal samples per pakke (10–1000)
extern volatile uint16_t current_sample_rate; // Aktuel samplerate i SPS (10–10000)

extern volatile uint8_t  bode_mode;
extern volatile uint8_t  bode_sample;
extern volatile uint8_t  bode_sample_ready;

void adc_init(uint16_t sample_rate, uint16_t rec_length);
void adc_set_params(uint16_t sample_rate, uint16_t rec_length);

#endif // ADC_H