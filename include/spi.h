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

void SPI_Init(SPI_Role role, uint8_t mode); // Initialisere SPI med Rolle (Master eller slave) and Mode (0-3, det er mest logisk at have samme mode på både master og slave)

uint8_t SPI_Transfer(uint8_t data); // Arbejdshesten: Sender én byte og modtager én ny byte hver gang

#endif