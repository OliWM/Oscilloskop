#include "UART.h"
#include <avr/interrupt.h>

// ---- Modtaget pakke (fyldes af ISR, læses af main) ----
volatile uint8_t pkt_type = 0;                 // BTN=0x01, SEND=0x02, START=0x03
volatile uint8_t pkt_data[PKT_MAX_DATA];       // n databytes
volatile uint8_t pkt_data_len = 0;             // antal gyldige databytes
volatile uint8_t ny_pakke_klar = 0;            // flag: main har en hel pakke at læse

// ---- Rå byte-ring (debug): hver eneste modtaget byte gemmes her, uanset format ----
#define RAW_RING_SZ 128                        // skal være en potens af 2 (maske nedenfor)
static volatile uint8_t raw_ring[RAW_RING_SZ];
static volatile uint8_t raw_w = 0;             // skrive-index (kun ISR)
static uint8_t          raw_r = 0;             // læse-index (kun main)

// Hent næste rå byte fra ringen. Returnerer -1 hvis der ikke er flere.
int16_t uart_raw_get(void)
{
    if (raw_r == raw_w) return -1;             // tom
    uint8_t c = raw_ring[raw_r];
    raw_r = (raw_r + 1) & (RAW_RING_SZ - 1);
    return c;
}

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

// Binær pakke-parser som state-machine. Pakkeformat:
//   0x55 0xAA | len_hi len_lo | type | data... | crc_hi crc_lo
// Sync-bytsene (0x55AA) bruges til at "re-frame": går noget galt, finder vi
// bare den næste 0x55 0xAA og starter forfra. CRC læses men valideres ikke (endnu).
ISR(USART0_RX_vect) {
    uint8_t c = UDR0;
    PINB |= (1 << PB7); //blink LED ved modtaget data

    // Gem RÅ byte i ringen (debug) FØR vi parser - så vi altid fanger alt.
    raw_ring[raw_w] = c;
    raw_w = (raw_w + 1) & (RAW_RING_SZ - 1);

    // Vi sætter kun ny_pakke_klar i den sidste state, og state er da allerede 0.
    // Så hvis main ikke har nået at læse, smider vi nye bytes væk indtil flaget er ryddet.
    if (ny_pakke_klar) return;

    enum { S_SYNC1, S_SYNC2, S_LEN_HI, S_LEN_LO, S_TYPE, S_DATA, S_CRC_HI, S_CRC_LO };
    static uint8_t  state = S_SYNC1;
    static uint16_t length = 0;     // rå længdefelt fra pakken
    static uint16_t remaining = 0;  // databytes der mangler at blive læst
    static uint8_t  idx = 0;        // skrive-index i pkt_data

    switch (state) {
    case S_SYNC1:
        if (c == 0x55) state = S_SYNC2;
        break;

    case S_SYNC2:
        if (c == 0xAA)      state = S_LEN_HI;   // sync komplet
        else if (c == 0x55) state = S_SYNC2;    // bliv og vent på 0xAA
        else                state = S_SYNC1;    // falsk start, søg igen
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
        // Viser det sig at length kun dækker payload, så skift linjen nedenfor til: remaining = length;
        remaining = (length >= 7) ? (length - 7) : 0;
        if (remaining > PKT_MAX_DATA) remaining = PKT_MAX_DATA; // klem fast så vi ikke render over bufferen
        pkt_data_len = (uint8_t)remaining;
        idx = 0;
        state = (remaining > 0) ? S_DATA : S_CRC_HI;
        break;

    case S_DATA:
        pkt_data[idx++] = c;
        if (--remaining == 0) state = S_CRC_HI;
        break;

    case S_CRC_HI:
        state = S_CRC_LO;       // CRC ignoreres lige nu
        break;

    case S_CRC_LO:
        ny_pakke_klar = 1;      // hel pakke klar til main
        state = S_SYNC1;        // klar til næste pakke
        break;
    }
}
