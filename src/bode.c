#include "bode.h"
#include "spi.h"
#include "UART.h"
#include "ADC.h"
#include <avr/io.h>
#include <util/delay.h>

void run_bode_sweep(uint8_t initial_freq)
{
    SigGen_SendParam(SPI_ADDR_SHAPE, 3);      // Sinus til bode sweep
    SigGen_SendParam(SPI_ADDR_AMPL, 255);     // Fuld amplitude
    SigGen_SendParam(SPI_ADDR_FREQ, initial_freq);
    _delay_ms(50);

    uint8_t bode_measurement[255];

    TIMSK1 &= ~(1 << OCIE1A); // Sluk timer adc samplingen

    for (uint8_t i = 1; ; i++)
    {
        SigGen_SendParam(SPI_ADDR_FREQ, i);   // Kun frekvens ændres i sweep
        _delay_ms(50);

        uint8_t max_ampl = 0;
        uint8_t min_ampl = 255;

        bode_mode = 1;
        bode_sample_ready = 0;
        ADCSRA |= (1 << ADSC);

        for (uint16_t j = 0; j < 500; j++)
        {
            while (!bode_sample_ready);
            bode_sample_ready = 0;
            uint8_t sample = bode_sample;

            if (sample > max_ampl)
                max_ampl = sample;
            if (sample < min_ampl)
                min_ampl = sample;
        }

        bode_mode = 0;
        bode_measurement[i - 1] = max_ampl - min_ampl;

        if (i == 255)
            break;
    }

    uint8_t reference = bode_measurement[0];
    if (reference > 0) {
        for (uint8_t i = 0; i < 255; i++)
            bode_measurement[i] = (uint16_t)bode_measurement[i] * 255 / reference;
    }

    TIMSK1 |= (1 << OCIE1A); // tænd timeren igen

    send_bode_packet(bode_measurement);
}
