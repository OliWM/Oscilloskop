#ifndef ADC_H
#define ADC_H

#include <stdint.h>

// Maksimal record length (allokeret bufferstørrelse)
#define MAX_RECORD_LENGTH 1000

// Double buffers
extern volatile uint8_t buffer_A[MAX_RECORD_LENGTH];
extern volatile uint8_t buffer_B[MAX_RECORD_LENGTH];
extern volatile uint8_t *active_buffer;    // ISR skriver hertil
extern volatile uint8_t *ready_buffer;     // Klar til afsendelse
extern volatile uint8_t  buffer_ready;     // Flag: klar buffer venter på afsendelse
extern volatile uint16_t buffer_index;
extern volatile uint16_t record_length;    // Antal samples per pakke (10–1000)

// Initialiserer ADC og Timer1
// sample_rate: ønsket samplerate i SPS (10–10000)
// rec_length:  antal samples per pakke (10–1000)
void adc_init(uint16_t sample_rate, uint16_t rec_length);

// Opdaterer samplerate og record length under kørsel
void adc_set_params(uint16_t sample_rate, uint16_t rec_length);

#endif // ADC_H