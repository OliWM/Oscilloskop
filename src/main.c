#include <avr/io.h>
#include <avr/interrupt.h>
#include "UART.h"
#include <stdio.h>
#include <stdlib.h>
#include <util/delay.h>

#define BAUD 115200
#define UBRR_VAL ((F_CPU / 8 / BAUD) - 1) //"reverse" formlen for baudrate


static void handle_uart_cmd(int16_t target_smooth[4], uint8_t uart_active[4], int16_t smooth_values[4])
{
    char cmd[16];
    cli();
    memcpy(cmd, (const void *)rx_buffer, 16);
    ny_data_klar = 0;
    sei();

    printString("\r\n"); // ISR echoer ikke newline; tilføj selv før svar

    char buf[24];
    sprintf(buf, "OK");
    printString(buf);
}
void send_generator_packet(void)
{
    // 55 AA 00 0B 01 00 00 00 00 00 00  (11 bytes total)
    putchUSART0(0x55);
    putchUSART0(0xAA); // sync
    putchUSART0(0x00);
    putchUSART0(0x0B); // length = 11
    putchUSART0(0x01); // type: GENERATOR
    putchUSART0(0x00);
    putchUSART0(0x00); // active, shape
    putchUSART0(0x00);
    putchUSART0(0x00); // amplitude, frequency
    putchUSART0(0x00);
    putchUSART0(0x00); // CRC (ZERO16)
}

void main() {
    DDRB |= (1 << PB7); //LED output
    uart0_Init(UBRR_VAL);
    sei(); 
    printString("\r\nTest: ");
    while(1){
        send_generator_packet();
        _delay_ms(1000);
       // if (ny_data_klar)
       // {
       //     char echo[16];
       //     cli();
       //     memcpy(echo, (const void *)rx_buffer, 16);
       //     ny_data_klar = 0;
       //     sei();
//
       //     printString("\r\nEcho: ");
       //     printString(echo);
       //     printString("\r\n");
       // }
}
};
