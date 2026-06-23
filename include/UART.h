#ifndef UART_H_
#define UART_H_

#include <avr/io.h>
#include <stdint.h>

#define PKT_MAX_DATA 64   // max antal databytes vi gemmer fra én pakke

// Pakke-typer
#define PKT_TYPE_BTN   0x01
#define PKT_TYPE_SEND  0x02
#define PKT_TYPE_START 0x03
#define PKT_TYPE_ADC   0x02   // ADC måledata fra MCU til LabVIEW

extern volatile uint8_t pkt_type;
extern volatile uint8_t pkt_data[PKT_MAX_DATA];
extern volatile uint8_t pkt_data_len;
extern volatile uint8_t ny_pakke_klar;

void uart0_Init(unsigned int ubrr);
void putchUSART0(char tx);
void printString(const char* s);
int16_t uart_raw_get(void);
void uart_send_adc_packet(volatile uint8_t *data, uint16_t length);

void UART_data_rx(uint8_t c);
void send_generator_packet(uint8_t active, uint8_t shape, uint8_t amplitude, uint8_t frequency);
void send_bode_packet(uint8_t *measurement);

#endif
