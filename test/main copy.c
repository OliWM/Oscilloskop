#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include "spi.h"
#include "I2C.h"
#include "ssd1306.h"

int main(void) {
    uint8_t high_byte, low_byte;
    uint16_t raw_temp;
    float celsius;
    char display_buffer[16];

    I2C_Init();

    InitializeDisplay();
    clear_display();
    
    SPI_Init(SPI_MASTER, 3); // Initialiseret som master og sat til mode 3, da vi ikke kan bruge mode 0, grundet et benspænd i opgaven der sagde et vi skulle bruge CPOL=1
    // MAX6675 er fixed i (mode 0) som slave og passer smooth som et tandhjul med (mode 0) master, så det havde været den mest optimale setting 

    while(1) {        
        
        SPI_PORT &= ~(1 << SPI_SS_PIN); // Pull SS Low (Begin SPI Transaction), så MAX6675 går i SPI mode og forbereder data til MISO på falling edge (setup)
        
        _delay_us(10); // mikrodelay for at MAX6675 kan nå at stabilisere sine pins og forberede data til MISO

        high_byte = SPI_Transfer(0xFF); // læser 8 ud af 16 bits i alt fra MISO PIN fra MAX6675, først de høje bits 
        low_byte  = SPI_Transfer(0xFF); // så de lave bits

        SPI_PORT |= (1 << SPI_SS_PIN); // Pull SS High (End SPI Transaction), så MAX6675 kan måle ny temperatur før næste overførelse

        raw_temp = (high_byte << 8) | low_byte; // Samler de to aflæsninger til 16 bit pakke
        raw_temp &= 0x7FFC; // Bit-maskering: Beholder bit 14-2 (Temperatur + Error). Fjerner bit 15, 1 og 0. (0x7FFC = 0111 1111 1111 1100 binært)

        if (raw_temp & 0x04) {   // Tjekker om thermocoupleren stadig er forbundet
            sprintf(display_buffer, "NO SENSOR");
        } else {
            celsius = (raw_temp >> 3) * 0.25; // Skifter 3 bits til højre, så første bit af 12-bit temp. 
            // målingen står først og ganges med 0.25, fordi det er det hver bit i aflæsningen svare til
            
            int heltalsdel = (int)celsius;
            int decimaldel = (int)((celsius - heltalsdel) * 100); // Giver de to første decimaler

sprintf(display_buffer, "Temp: %d.%02d C", heltalsdel, decimaldel);   // printer ind i array som float
        }

        sendStrXY(display_buffer, 4, 0); // Opdaterer display

        _delay_ms(300); // Delay fordi MAX6675 tager 0.22 sek. om at behandle ny måling fra thermocoupleren

    }
}