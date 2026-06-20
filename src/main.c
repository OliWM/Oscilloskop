#include <avr/io.h>
#include <avr/interrupt.h>
#include "UART.h"
#include "ADC.h"
#include "timer.h"
#include "I2C.h"
#include "ssd1306.h"
#include "display.h"
#include <stdio.h>
#include <stdlib.h>
#include <util/delay.h>
#include "spi.h"
#include "bode.h"

#define BAUD 115200
#define UBRR_VAL ((F_CPU / 8 / BAUD) - 1) //"reverse" formlen for baudrate

#define SPI_test_mode 0

// Generator-tilstand
static uint8_t  setting   = 0;    // 0=shape, 1=amplitude, 2=frequency
static uint8_t  shape     = 0;
static uint8_t  amplitude = 0;
static uint8_t  frequency = 0;
static uint8_t  measuring = 1;    // toggles ved hvert RUN-tryk (BTN2)
static int16_t  last_spi_status = -1;  // -1 = ingen SPI-overførsel endnu

static void handle_button(uint8_t button, uint8_t sw)
{
    if (button == 1) {            // SELECT (BTN1): skift indstilling 0->1->2->0
        setting = (setting + 1) % 3;
    } else if (button == 0) {     // ENTER (BTN0): gem SW-værdien i valgt indstilling
        if (setting == 0)      shape     = sw;
        else if (setting == 1) amplitude = sw;
        else                   frequency = sw;
        last_spi_status = (int16_t)SigGen_Update(shape, amplitude, frequency);
    }
    else if (button == 3)
    { // RESET (BTN3): nulstil alle parametre
        shape = amplitude = frequency = 0;
        last_spi_status = (int16_t)SigGen_Update(shape, amplitude, frequency);
    }
    else if (button == 2)
    { // RUN (BTN2): toggle measuring-flag
        measuring = !measuring;
    }
    send_generator_packet(setting, shape, amplitude, frequency);
}

static void handle_spi_test(void)
{
    uint16_t hs_count = 0;
    for (uint16_t i = 0; i < 10000; i++)
    {
        last_spi_status = (int16_t)SigGen_Update(shape, amplitude, frequency);
        if (last_spi_status == 0x55) hs_count++;
    }
    char spi_result1[6];
    char spi_result2[6];
    char spi_result3[8];
    sprintf(spi_result1, "SPI: ");
    sprintf(spi_result2, "%5d", hs_count);
    sprintf(spi_result3, " /10000");
    clear_display();
    sendStrXY(spi_result1, 3, 4);
    sendStrXY(spi_result2, 4, 4);
    sendStrXY(spi_result3, 5, 2);
}

int main(void) {
    DDRB |= (1 << PB7); //LED output
    uart0_Init(UBRR_VAL);
    SPI_Init(SPI_MASTER, 0);
    I2C_Init();
    clear_display();
    InitializeDisplay();

    sei();
    adc_init(1000, 100);  // default: 1000 SPS, 100 samples per pakke
    send_generator_packet(0, 0, 0, 0);   // bed den anden enhed om at sende (start-tilstand)

    uint8_t  scroll_top = 0;
    uint8_t  display_dirty = 1;
    uint8_t  loop_counter  = 0;

    while (1) {
        int16_t b;

        while((b = uart_raw_get()) >= 0)
            UART_data_rx((uint8_t)b);

        if (ny_pakke_klar) {
            char    line[LINE_W + 1];
            uint8_t type   = pkt_type;
            format_hex_line(line, pkt_data, pkt_data_len);
            ny_pakke_klar = 0;

            if (type == PKT_TYPE_BTN) {
                if (!SPI_test_mode)
                    handle_button(pkt_data[0], pkt_data[1]);
                if (SPI_test_mode)
                    handle_spi_test();
            }
            else if (type == PKT_TYPE_SEND) {
                uint16_t sample_rate = ((uint16_t)pkt_data[0] << 8) | pkt_data[1];
                uint16_t rec_length  = ((uint16_t)pkt_data[2] << 8) | pkt_data[3];
                adc_set_params(sample_rate, rec_length);
            }
            else if (type == PKT_TYPE_START) {
                shape = 3;
                amplitude = 255;
                run_bode_sweep(frequency);
            }

            log_add(line);
            scroll_top = (hist_count > LOG_ROWS) ? (hist_count - LOG_ROWS) : 0;
            display_dirty = 1;
        }

        if (measuring && buffer_ready) {
            uart_send_adc_packet(ready_buffer, record_length);
            buffer_ready = 0;
        }

        if (++loop_counter >= DISPLAY_UPDATE_INTERVAL) {
            loop_counter = 0;
            if (display_dirty && !SPI_test_mode) {
                display_dirty = 0;
                draw_window(scroll_top);
                update_status_lines(last_spi_status);
            }
        }
    }
}