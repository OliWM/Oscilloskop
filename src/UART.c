#include "UART.h"
#include <avr/interrupt.h>

volatile char rx_buffer[16]; 
volatile uint8_t rx_pos = 0;
volatile uint8_t ny_data_klar = 0;  // flag der fortæller main om den skal læse ny data

void uart0_Init(unsigned int ubrr) {
    UBRR0H = (unsigned char)(ubrr >> 8); //Indlæser MSB 8 bit (0'er)
    UBRR0L = (unsigned char)ubrr; //Indlæser LSB 8 bit (16)
    UCSR0A = (1 << U2X0); //double speed
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0); //enable RX og TX, RX Complete Interrupt enable
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); //1 startbit, 8 databit, 1 stopbit
}

void putchUSART0(char tx) { //skal laves til et interrupt tror jeg
    while (!(UCSR0A & (1 << UDRE0))); // Sidder fast i while-løkken så længe der IKKE er empty i dataregisteret (polling)
    UDR0 = tx;  // flytter alle 8 bits ind i UDR0 TX registeret - UART sender automatisk serielt afsted når noget rykkes ind i tx
}

void printString(const char* s) {   // pointer til char array
    while (*s) putchUSART0(*s++); // dereferencer, så længe der "er" noget -> send char og increment array index
}

ISR(USART0_RX_vect) {
    char c = UDR0;

    // Hvis vi allerede har data, der venter på main, så stopper den med at modtage
    if (ny_data_klar) return; 

    if (c == '\n' || c == '\r') {
        if (rx_pos > 0) { // Kun hvis vi faktisk har modtaget tegn
            rx_buffer[rx_pos] = '\0'; 
            ny_data_klar = 1;         
            rx_pos = 0;
        }
    } 
    else if (rx_pos < 15) { // håndtere buffer overrun
        putchUSART0(c); // Echo tegnet tilbage til monitor, så vi kan se hvad vi skriver
        rx_buffer[rx_pos++] = c;    // gemmer i buffer
    }
}