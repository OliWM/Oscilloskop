#ifndef SPI_H_
#define SPI_H_

#include <avr/io.h>

// Mega 2560 SPI Pin navne for at gøre det nemmere at huske forbindelserne
#define SPI_SS_PIN   PB0  // 53
#define SPI_SCK_PIN  PB1 // 52
#define SPI_MOSI_PIN PB2 // 51
#define SPI_MISO_PIN PB3 // 50
#define SPI_DDR      DDRB
#define SPI_PORT     PORTB

typedef enum {
    SPI_MASTER,
    SPI_SLAVE
} SPI_Role;

#define SPI_SYNC      0xA5
#define SPI_ADDR_SHAPE 0x01
#define SPI_ADDR_AMPL  0x02
#define SPI_ADDR_FREQ  0x03

void SPI_Init(SPI_Role role, uint8_t mode);

uint8_t SPI_Transfer(uint8_t data);

uint8_t SigGen_SendParam(uint8_t address, uint8_t data);

#endif