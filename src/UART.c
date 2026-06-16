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
static volatile uint8_t raw_w = 0;             // skrive-index (kun ISR) (producer)
static uint8_t          raw_r = 0;             // læse-index (kun main) (consumer)

// Hent næste rå byte fra ringen. Returnerer -1 hvis der ikke er flere.
int16_t uart_raw_get(void)
{
    if (raw_r == raw_w) return -1; // tom

    uint8_t c = raw_ring[raw_r];
    raw_r = (raw_r + 1) & (RAW_RING_SZ - 1); // AND'er , så hvis raw_r < 128, no problem, men ved raw_r < 128 bliver det 10000000 & 01111111 = 0, altså den resetter. Ku have brugt modulo
    return c;
}

void uart0_Init(unsigned int ubrr) {
    UBRR0H = (unsigned char)(ubrr >> 8); //Indlæser MSB 8 bit (0'er)
    UBRR0L = (unsigned char)ubrr; //Indlæser LSB 8 bit (16)
    UCSR0A = (1 << U2X0); //double speed
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0); //enable RX og TX, RX Complete Interrupt enable
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); //1 startbit, 8 databit, 1 stopbit
}  

void putchUSART0(char tx) { 
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
    if (raw_w != ((raw_r - 1) & (RAW_RING_SZ - 1))){ //tjekker at ringen ikke er fuld (at write har indhentet read med en fuld omgang)
    raw_ring[raw_w] = c;
    raw_w = (raw_w + 1) & (RAW_RING_SZ - 1);
    }
    // skal vi markere at vi mister data somehow.. så vi ved det?
}


    // ---------------------------------------------------------------------------
    // Send ADC-buffer som binær pakke til LabVIEW
    // Pakkeformat: 0x55 0xAA | len_hi len_lo | type | data... | crc_hi crc_lo
    // length = total pakkelængde inkl. alle overhead-bytes (7 + databytes)
    // ---------------------------------------------------------------------------
    void uart_send_adc_packet(volatile uint8_t *data, uint16_t length)
    {
        uint16_t total_length = length + 7; // 2 sync + 2 len + 1 type + 2 crc

        // Sync bytes
        putchUSART0(0x55);
        putchUSART0(0xAA);

        // Length (total pakkelængde)
        putchUSART0((uint8_t)(total_length >> 8));
        putchUSART0((uint8_t)(total_length & 0xFF));

        // Type
        putchUSART0(PKT_TYPE_ADC);

        // Data
        for (uint16_t i = 0; i < length; i++)
        {
            putchUSART0(data[i]);
        }

        // CRC placeholder
        putchUSART0(0x00);
        putchUSART0(0x00);
    }

    /*Tilføjet selve uart_send_adc_packet() funktionen der bygger pakken op byte for byte:
    Sender sync bytes 0x55 0xAA
    Sender total længde (data + 7 overhead bytes) som to bytes
    Sender PKT_TYPE_ADC som type
    Looper igennem databufferen og sender alle samples
    Sender 0x00 0x00 som CRC placeholder*/