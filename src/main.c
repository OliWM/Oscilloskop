#include <avr/io.h>
#include <avr/interrupt.h>
#include "UART.h"
#include "ADC.h"
#include "timer.h"
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
#define SPI_test_mode 0

// ---- OLED layout ----
#define LINE_W   16     // skærmen er 16 tegn bred
#define SCREEN_H 8      // skærmen er 8 linjer høj (0-7)
#define HIST     32     // hvor mange linjer historik vi gemmer (rul-log)

// De øverste LOG_ROWS linjer er rul-log, de sidste 3 er status (S:/SR:/RL:).
#define LOG_ROWS      5
#define ROW_STATUS_S  5
#define ROW_STATUS_SR 6
#define ROW_STATUS_RL 7

// OLED-skrivning over I2C er langsom — opdater skærmen kun hvert N'te
// gennemløb af hovedløkken (når der faktisk er noget nyt at vise).
#define DISPLAY_UPDATE_INTERVAL 20

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

// Skriv 'text' på 'row', paddet med mellemrum til fuld bredde så gamle tegn overskrives.
static void print_padded(uint8_t row, const char *text)
{
    char padded[LINE_W + 1];
    uint8_t i = 0;
    while (text[i] && i < LINE_W) { padded[i] = text[i]; i++; }
    while (i < LINE_W) padded[i++] = ' ';
    padded[LINE_W] = '\0';
    sendStrXY(padded, row, 0);   // X = række, Y = kolonne (0)
}

// Tegn et vindue på LOG_ROWS linjer fra historikken, startende ved 'top'.
static void draw_window(uint8_t top)
{
    for (uint8_t row = 0; row < LOG_ROWS; row++) {
        uint8_t idx = top + row;
        print_padded(row, (idx < hist_count) ? history[idx] : "");
    }
}

// Statuslinjer i bunden af skærmen: sidste SPI-status, samplerate, record length.
static void update_status_lines(int16_t spi_status)
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

void send_bode_packet(uint8_t *measurement)
{
    putchUSART0(0x55);
    putchUSART0(0xAA); // sync
    putchUSART0(0x01);
    putchUSART0(0x06); // length = 262
    putchUSART0(0x03); // type: BODE
    for (uint8_t i = 0; i < 255; i++){
        putchUSART0(measurement[i]);
    }
    putchUSART0(0x00);
    putchUSART0(0x00); // CRC (ZERO16)
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
enum
{
    S_SYNC1,
    S_SYNC2,
    S_LEN_HI,
    S_LEN_LO,
    S_TYPE,
    S_DATA,
    S_CRC_HI,
    S_CRC_LO
};

void UART_data_rx(uint8_t c)
{
    static uint8_t state = S_SYNC1;
    static uint16_t length = 0;    // rå længdefelt fra pakken
    static uint16_t remaining = 0; // databytes der mangler at blive læst
    static uint8_t idx = 0;        // skrive-index i pkt_data


    switch (state)
    {
    case S_SYNC1:
        if (c == 0x55)
            state = S_SYNC2;
        break;

    case S_SYNC2:
        if (c == 0xAA)
            state = S_LEN_HI; // sync komplet
        else if (c == 0x55)
            state = S_SYNC2; // bliv og vent på 0xAA
        else
            state = S_SYNC1; // falsk start, søg igen
        break;

    case S_LEN_HI:
        length = ((uint16_t)c) << 8;
        state = S_LEN_LO;
        break;

    case S_LEN_LO:
        length |= c;
        state = S_TYPE;
        break;

    case S_TYPE:
        pkt_type = c;
        // Antager length = HELE pakkens længde:
        //   2 sync + 2 len + 1 type + 2 crc = 7 overhead-bytes -> data = length - 7.
        remaining = (length >= 7) ? (length - 7) : 0;
        if (remaining > PKT_MAX_DATA)
            remaining = PKT_MAX_DATA; // klem fast så vi ikke render over bufferen
        pkt_data_len = (uint8_t)remaining;
        idx = 0;
        state = (remaining > 0) ? S_DATA : S_CRC_HI;
        break;

    case S_DATA:
        pkt_data[idx++] = c;
        if (--remaining == 0)
            state = S_CRC_HI;
        break;

    case S_CRC_HI:
        state = S_CRC_LO; // CRC ignoreres lige nu
        break;

    case S_CRC_LO:
        ny_pakke_klar = 1; // hel pakke klar til main
        state = S_SYNC1;   // klar til næste pakke
        break;
    }
}

void main() {
    DDRB |= (1 << PB7); //LED output
    uart0_Init(UBRR_VAL);
    SPI_Init(SPI_MASTER, 0);
    I2C_Init();
    clear_display();
    InitializeDisplay();

    sei();
    adc_init(1000, 100);  // default: 1000 SPS, 100 samples per pakke
    send_generator_packet(0, 0, 0, 0);   // bed den anden enhed om at sende (start-tilstand)

    uint8_t  scroll_top = 0;   // øverste synlige linje (nederste linje = nyeste)

    // Generator-tilstand. 'setting' (active indicator) vælger hvilket felt ENTER skriver til.
    uint8_t  setting   = 0;    // 0=shape, 1=amplitude, 2=frequency
    uint8_t  shape     = 0;
    uint8_t  amplitude = 0;
    uint8_t  frequency = 0;
    uint8_t  measuring = 1;    // toggles ved hvert RUN-tryk (BTN2), starter slået fra
    int16_t  last_spi_status = -1;  // -1 = ingen SPI-overførsel endnu
    uint8_t  display_dirty = 1;     // tegn skærmen mindst én gang ved opstart
    uint8_t  loop_counter  = 0;     // tæller løkkegennemløb mellem skærm-opdateringer

#if RAW_DEBUG
    static const char hexd[] = "0123456789ABCDEF";
    char    rawcur[LINE_W + 1]; // linje vi bygger op af indkommende hex
    uint8_t rawlen = 0;         // antal tegn i rawcur lige nu
    uint8_t idle   = 0;         // hvor mange polls siden sidste byte (til flush)
#endif

    while (1) {
        int16_t b; //16 fordi værdien både kan være -1 (så ikke uint) og op til 255 (så over 127 vi ku få fra int8_t)

        while((b = uart_raw_get()) >= 0){ //sætter b= uart_raw_get() og tjekker om der er noget
            UART_data_rx((uint8_t)b);
        }

        // send_generator_packet(setting, shape, amplitude, frequency); //stop timeout
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
        if (ny_pakke_klar) { //lav som funktion og fjern fra main?
            char    line[LINE_W + 1];
            uint8_t type   = pkt_type;
            format_hex_line(line, pkt_data, pkt_data_len);
            ny_pakke_klar = 0;     // frigiv ISR'en til at modtage næste pakke

            // Knaptryk -> opdater generator-tilstand og send et GENERATOR-svar tilbage.
            if (type == PKT_TYPE_BTN) {
                if (!SPI_test_mode){
                uint8_t button = pkt_data[0]; // ved BTN: byte 0 = knap (0-3)
                uint8_t sw = pkt_data[1];     // ved BTN: byte 1 = SW-værdi (0-255)
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
                { // RUN (BTN2): toggle measuring-flag, ellers ingenting
                    measuring = !measuring;
                }
                send_generator_packet(setting, shape, amplitude, frequency);
                }
                if (SPI_test_mode){
                    uint16_t hs_count = 0;
                    for (uint16_t i = 0; i < 10000; i++)
                    {
                        last_spi_status = (int16_t)SigGen_Update(shape, amplitude, frequency);
                        if (last_spi_status == 0x55) hs_count++ ;
                    }
                    char spi_result[18];
                    sprintf(spi_result, "SPI: %5d /10000", hs_count);
                    clear_display();
                    sendStrXY(spi_result, 0, 4);
                }
            }
            else if (type == PKT_TYPE_SEND){ //oscilloscop instruks
                // Data: [0..1] = sample rate (big-endian), [2..3] = record length (big-endian)
                uint16_t sample_rate = ((uint16_t)pkt_data[0] << 8) | pkt_data[1];
                uint16_t rec_length  = ((uint16_t)pkt_data[2] << 8) | pkt_data[3];
                
                adc_set_params(sample_rate, rec_length); // klemmer værdierne til sikre grænser internt
            }
            else if (type == PKT_TYPE_START) //bodeplot
            {
                shape = 3; //Sinuskurve
                amplitude = 255; //max
                SigGen_Update(shape, amplitude, frequency);
                _delay_ms(50); // giv den lige tid til at indstille det nye signal. 50ms>1 periode selv v 24hz
                
                uint8_t bode_measurement[255];

                TIMSK1 &= ~(1 << OCIE1A); // Sluk timer adc samplingen

                for (uint8_t i = 1; ; i++) //wrapper i ved 255??
                {
                    SigGen_Update(shape, amplitude, i);
                    _delay_ms(50);

                    uint8_t max_ampl = 0;
                    uint8_t min_ampl = 255;

                    for (uint16_t j = 0; j < 500; j++) //tag 500 målinger
                    {
                        ADCSRA |= (1 << ADSC); //Start konvertering
                        while (ADCSRA & (1<<ADSC)); // vent til sample færdigt

                        uint8_t sample = ADCH; 
                        if (sample > max_ampl){ //tager allerhøjeste - risiko for støj
                            max_ampl = sample;
                    }
                        if (sample < min_ampl){
                            min_ampl = sample;
                        }
                    }
                            bode_measurement[i - 1] = max_ampl - min_ampl;

                            if (i == 255){
                                break; // forsøg på at undgå wrap?
                            }
                    }
                    uint8_t reference = bode_measurement[0];
                    if (reference > 0){ //hvis det ikk har virket
                        for (uint8_t i = 0; i < 255; i++){
                            bode_measurement[i] = (uint16_t)bode_measurement[i] * 255 / reference; //gør alle relative til referencen
                        }
                    }
                    TIMSK1 |= (1 << OCIE1A); //tænd timeren igen

                    send_bode_packet(bode_measurement);
            }

            log_add(line);
            // Hop til bunden så den nyeste pakke altid er synlig
            scroll_top = (hist_count > LOG_ROWS) ? (hist_count - LOG_ROWS) : 0;
            display_dirty = 1;   // skærmen tegnes først når DISPLAY_UPDATE_INTERVAL er nået
        }
#endif
        // Send ADC-data når en buffer er klar og måling er aktiv
        if (measuring && buffer_ready) {
            uart_send_adc_packet(ready_buffer, record_length);
            buffer_ready = 0;
        }

        // OLED over I2C er langsom — tegn den kun hvert DISPLAY_UPDATE_INTERVAL'te
        // gennemløb, og kun hvis der faktisk er noget nyt (display_dirty).
        if (++loop_counter >= DISPLAY_UPDATE_INTERVAL) {
            loop_counter = 0;
            if (display_dirty && !SPI_test_mode) {
                display_dirty = 0;
                draw_window(scroll_top);
                update_status_lines(last_spi_status);
            }
        }

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
/*#include "adc.h" og #include "timer.h" tilføjet øverst
adc_init(1000, 100) kaldt efter sei()
ADC-afsendelse i bunden af loop — kun når både measuring og buffer_ready er sat*/