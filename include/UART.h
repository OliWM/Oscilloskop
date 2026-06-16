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

// Sættes af RX-ISR'en, læses af main. Læs dem mens ny_pakke_klar==1; ISR'en
// rører dem ikke før main har ryddet flaget igen.
extern volatile uint8_t pkt_type;
extern volatile uint8_t pkt_data[PKT_MAX_DATA];
extern volatile uint8_t pkt_data_len;
extern volatile uint8_t ny_pakke_klar;
extern volatile uint8_t UART_flag;

void uart0_Init(unsigned int ubrr);
void putchUSART0(char tx);
void printString(const char* s);
int16_t uart_raw_get(void);   // debug: hent næste rå modtaget byte (-1 = tom)

// Send ADC-buffer som binær pakke til LabVIEW
// Format: 0x55 0xAA | len_hi len_lo | type | data... | 0x00 0x00 (CRC placeholder)
void uart_send_adc_packet(volatile uint8_t *data, uint16_t length);

#endif

/*Tilføjet #define PKT_TYPE_ADC 0x04 som ny pakketype
Tilføjet declaration af uart_send_adc_packet()*/