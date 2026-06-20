#include "spi.h"

void SPI_Init(SPI_Role role, uint8_t mode) {
    if (role == SPI_MASTER) {
        SPI_DDR |= (1 << SPI_MOSI_PIN) | (1 << SPI_SCK_PIN) | (1 << SPI_SS_PIN);    // Sætter MOSI, SCK og SS som outputs
        SPI_PORT |= (1 << SPI_SS_PIN);  // Sikre at SS pin er høj fra start (IDLE)
        SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1); // Enable SPI, Master
        SPSR |= (1 << SPI2X);                          // SPI2X=1, SPR1=1, SPR0=0, set clock rate fosc/32 = 500 kHz
    } else {
        SPI_DDR |= (1 << SPI_MISO_PIN); // Slave Mode: MISO is output, others are input
        SPCR = (1 << SPE);  // Enable SPI, Slave
    }

    // Set SPI Mode (CPOL and CPHA)
    // Mode 0: CPOL=0, CPHA=0 | Mode 1: CPOL=0, CPHA=1
    // Mode 2: CPOL=1, CPHA=0 | Mode 3: CPOL=1, CPHA=1

    switch(mode) {
        case 1: SPCR |= (1 << CPHA); break;
        case 2: SPCR |= (1 << CPOL); break;
        case 3: SPCR |= (1 << CPOL) | (1 << CPHA); break;
        default: break; // Mode 0
    }
}

uint8_t SPI_Transfer(uint8_t data) {    
    SPDR = data;
    while(!(SPSR & (1 << SPIF)));
    return SPDR;
}

uint8_t SigGen_Update(uint8_t shape, uint8_t ampl, uint8_t freq)
{
    SPI_PORT &= ~(1 << SPI_SS_PIN); // SS lav — start transaktion
    uint8_t hs = SPI_Transfer(0xAA);             // sync byte, læs handshake ind på hs
    SPI_Transfer(shape);
    SPI_Transfer(ampl);
    SPI_Transfer(freq);
    SPI_PORT |= (1 << SPI_SS_PIN); // SS høj — FPGA latcher værdierne her

    return hs;
}