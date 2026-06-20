#include "display.h"
#include "ssd1306.h"
#include "ADC.h"
#include <stdio.h>
#include <string.h>

static char history[HIST][LINE_W + 1];
uint8_t hist_count = 0;

static void print_padded(uint8_t row, const char *text)
{
    char padded[LINE_W + 1];
    uint8_t i = 0;
    while (text[i] && i < LINE_W) { padded[i] = text[i]; i++; }
    while (i < LINE_W) padded[i++] = ' ';
    padded[LINE_W] = '\0';
    sendStrXY(padded, row, 0);
}

void log_add(const char *line)
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

void format_hex_line(char *out, const volatile uint8_t *data, uint8_t len)
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

void draw_window(uint8_t top)
{
    for (uint8_t row = 0; row < LOG_ROWS; row++) {
        uint8_t idx = top + row;
        print_padded(row, (idx < hist_count) ? history[idx] : "");
    }
}

void update_status_lines(int16_t spi_status)
{
    char line[LINE_W + 1];

    if (spi_status >= 0) {
        sprintf(line, "S:0x%02X", (uint8_t)spi_status);
        print_padded(ROW_STATUS_S, line);
    }
    sprintf(line, "SR:%u", current_sample_rate);
    print_padded(ROW_STATUS_SR, line);
    sprintf(line, "RL:%u", record_length);
    print_padded(ROW_STATUS_RL, line);
}
