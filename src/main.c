#include <avr/io.h>
#include "UART.h"

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

void main() {
    uart0_Init(UBRR_VAL);

    if (ny_data_klar)
    {
        // handle_uart_cmd(target_smooth, uart_active, smooth_values);
    }
};
