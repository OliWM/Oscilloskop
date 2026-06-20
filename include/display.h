#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#define LINE_W   16     // skærmen er 16 tegn bred
#define HIST     32     // hvor mange linjer historik vi gemmer (rul-log)

#define LOG_ROWS      5
#define ROW_STATUS_S  5
#define ROW_STATUS_SR 6
#define ROW_STATUS_RL 7

#define DISPLAY_UPDATE_INTERVAL 20

extern uint8_t hist_count;

void log_add(const char *line);
void format_hex_line(char *out, const volatile uint8_t *data, uint8_t len);
void draw_window(uint8_t top);
void update_status_lines(int16_t spi_status);

#endif
