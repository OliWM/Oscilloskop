#include <avr/io.h>
#include <avr/interrupt.h>
#include "UART.h"
#include "I2C.h"
#include "ssd1306.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util/delay.h>
#include <spi.h>

#define BAUD 115200
#define UBRR_VAL ((F_CPU / 8 / BAUD) - 1) //"reverse" formlen for baudrate

// Sæt til 1 for at vise ALLE rå modtagne bytes som hex (uanset pakkeformat).
// Sæt til 0 for normal pakke-visning. Pakke-parseren kører i begge tilfælde.
#define RAW_DEBUG 0

// ---- OLED layout ----
#define LINE_W   16     // skærmen er 16 tegn bred
#define SCREEN_H 8      // skærmen er 8 linjer høj (0-7)
#define HIST     32     // hvor mange linjer historik vi gemmer (rul-log)

// Rul-log: én linje pr. modtaget pakke. Når den er fuld, skubber vi op (FIFO).
static char    history[HIST][LINE_W + 1];
static uint8_t hist_count = 0;   // antal gyldige linjer (0..HIST)

// Læg en ny linje nederst i loggen. Er loggen fuld, skub alt en op først.
static void log_add(const char *line)
{
    if (hist_count < HIST) {
        strncpy(history[hist_count], line, LINE_W);
        history[hist_count][LINE_W] = '\0';
        hist_count++;
    } else {
        for (uint8_t i = 1; i < HIST; i++)
            memcpy(history[i - 1], history[i], LINE_W + 1);
        strncpy(history[HIST - 1], line, LINE_W);
        history[HIST - 1][LINE_W] = '\0';
    }
}

// Formatér pakkens databytes som hex ind i out (16 tegn = 8 bytes uden mellemrum).
// Er der flere end 8 databytes, viser vi kun de første 8 på linjen.
static void format_hex_line(char *out, const volatile uint8_t *data, uint8_t len)
{
    static const char hexd[] = "0123456789ABCDEF";
    uint8_t n = (len > 8) ? 8 : len;
    uint8_t p = 0;
    for (uint8_t i = 0; i < n; i++) {
        out[p++] = hexd[(data[i] >> 4) & 0x0F];
        out[p++] = hexd[data[i] & 0x0F];
    }
    out[p] = '\0';
}

// Tegn et vindue på 8 linjer fra historikken, startende ved 'top'.
// Hver linje paddes med mellemrum til fuld bredde, så gamle tegn overskrives.
static void draw_window(uint8_t top)
{
    char padded[LINE_W + 1];
    for (uint8_t row = 0; row < SCREEN_H; row++) {
        uint8_t idx = top + row;
        uint8_t i = 0;
        if (idx < hist_count)
            while (history[idx][i] && i < LINE_W) { padded[i] = history[idx][i]; i++; }
        while (i < LINE_W) padded[i++] = ' ';
        padded[LINE_W] = '\0';
        sendStrXY(padded, row, 0);   // X = række (0-7), Y = kolonne (0)
    }
}

// GENERATOR-svar (type 0x01). Data = active, shape, amplitude, frequency (4 bytes).
// 55 AA | 00 0B(=11) | 01 | active shape amp freq | 00 00  (11 bytes total)
void send_generator_packet(uint8_t active, uint8_t shape, uint8_t amplitude, uint8_t frequency)
{
    putchUSART0(0x55);
    putchUSART0(0xAA);       // sync
    putchUSART0(0x00);
    putchUSART0(0x0B);       // length = 11
    putchUSART0(0x01);       // type: GENERATOR
    putchUSART0(active);
    putchUSART0(shape);
    putchUSART0(amplitude);
    putchUSART0(frequency);
    putchUSART0(0x00);
    putchUSART0(0x00);       // CRC (ZERO16)
}
void SigGen_Update(uint8_t shape, uint8_t ampl, uint8_t freq)
{
    SPI_PORT &= ~(1 << SPI_SS_PIN); // SS lav — start transaktion
    SPI_Transfer(0xAA);             // sync byte
    SPI_Transfer(shape);
    SPI_Transfer(ampl);
    SPI_Transfer(freq);
    SPI_PORT |= (1 << SPI_SS_PIN); // SS høj — FPGA latcher værdierne her
}

void main() {
    DDRB |= (1 << PB7); //LED output
    uart0_Init(UBRR_VAL);
    I2C_Init();
    clear_display();
    InitializeDisplay();

    sei();
    send_generator_packet(0, 0, 0, 0);   // bed den anden enhed om at sende (start-tilstand)

    uint8_t  scroll_top = 0;   // øverste synlige linje (nederste linje = nyeste)

    // Generator-tilstand. 'setting' (active indicator) vælger hvilket felt ENTER skriver til.
    uint8_t  setting   = 0;    // 0=shape, 1=amplitude, 2=frequency
    uint8_t  shape     = 0;
    uint8_t  amplitude = 0;
    uint8_t  frequency = 0;
    uint8_t  measuring = 0;    // toggles ved hvert RUN-tryk (BTN2), starter slået fra

#if RAW_DEBUG
    static const char hexd[] = "0123456789ABCDEF";
    char    rawcur[LINE_W + 1]; // linje vi bygger op af indkommende hex
    uint8_t rawlen = 0;         // antal tegn i rawcur lige nu
    uint8_t idle   = 0;         // hvor mange polls siden sidste byte (til flush)
#endif

    while (1) {
#if RAW_DEBUG
        // --- RÅ DEBUG: dump hver modtaget byte som hex, uanset format ---
        int16_t b;
        uint8_t got = 0;
        while ((b = uart_raw_get()) >= 0) {
            rawcur[rawlen++] = hexd[(b >> 4) & 0x0F];
            rawcur[rawlen++] = hexd[b & 0x0F];
            idle = 0;
            if (rawlen >= LINE_W) {          // linjen er fuld (8 bytes) -> commit
                rawcur[LINE_W] = '\0';
                log_add(rawcur);
                rawlen = 0;
                got = 1;
            }
        }
        if (got) {
            scroll_top = (hist_count > SCREEN_H) ? (hist_count - SCREEN_H) : 0;
            draw_window(scroll_top);
        }
#else
        // --- Ny pakke modtaget? Læg databytes som hex-linje i loggen ---
        if (ny_pakke_klar) {
            char    line[LINE_W + 1];
            uint8_t type   = pkt_type;
            uint8_t button = pkt_data[0];   // ved BTN: byte 0 = knap (0-3)
            uint8_t sw     = pkt_data[1];   // ved BTN: byte 1 = SW-værdi (0-255)
            format_hex_line(line, pkt_data, pkt_data_len);
            ny_pakke_klar = 0;     // frigiv ISR'en til at modtage næste pakke

            // Knaptryk -> opdater generator-tilstand og send et GENERATOR-svar tilbage.
            if (type == PKT_TYPE_BTN) {
                if (button == 1) {            // SELECT (BTN1): skift indstilling 0->1->2->0
                    setting = (setting + 1) % 3;
                } else if (button == 0) {     // ENTER (BTN0): gem SW-værdien i valgt indstilling
                    if (setting == 0)      shape     = sw;
                    else if (setting == 1) amplitude = sw;
                    else                   frequency = sw;
                    SigGen_Update(shape, amplitude, frequency); //SPI til FPGA
                }
                else if (button == 3)
                { // RESET (BTN3): nulstil alle parametre
                    shape = amplitude = frequency = 0;
                    SigGen_Update(shape, amplitude, frequency); // SPI til FPGA
                }
                else if (button == 2)
                { // RUN (BTN2): toggle measuring-flag, ellers ingenting
                    measuring = !measuring;
                }
                send_generator_packet(setting, shape, amplitude, frequency);
            }

            log_add(line);
            // Hop til bunden så den nyeste pakke altid er synlig
            scroll_top = (hist_count > SCREEN_H) ? (hist_count - SCREEN_H) : 0;
            draw_window(scroll_top);
        }
#endif

        _delay_ms(50);           // poll hurtigt nok til at fange nye pakker

#if RAW_DEBUG
        // Flush en ufærdig linje hvis strømmen har været stille (~200ms),
        // så hver burst af bytes bliver vist selvom den ikke fylder en hel linje.
        if (rawlen > 0 && ++idle >= 4) {
            rawcur[rawlen] = '\0';
            log_add(rawcur);
            rawlen = 0;
            idle = 0;
            scroll_top = (hist_count > SCREEN_H) ? (hist_count - SCREEN_H) : 0;
            draw_window(scroll_top);
        }
#endif

    }
}
