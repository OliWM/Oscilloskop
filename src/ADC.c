#include <avr/io.h>
#include <avr/interrupt.h>
#include "ADC.h"
#include "timer.h"

volatile uint8_t buffer_A[MAX_RECORD_LENGTH];
volatile uint8_t buffer_B[MAX_RECORD_LENGTH];
volatile uint8_t *active_buffer  = buffer_A;
volatile uint8_t *ready_buffer   = buffer_B;
volatile uint8_t  buffer_ready   = 0;
volatile uint16_t buffer_index   = 0;
volatile uint16_t record_length  = MAX_RECORD_LENGTH;
volatile uint16_t current_sample_rate = 0;

volatile uint8_t  bode_mode         = 0;
volatile uint8_t  bode_sample       = 0;
volatile uint8_t  bode_sample_ready = 0;

static uint16_t clamp_sample_rate(uint16_t sr)
{
    if (sr < 10) return 10;
    if (sr > 10000) return 10000;
    return sr;
}

static uint16_t clamp_record_length(uint16_t rl, uint16_t sr)
{
    uint32_t byte_rate = (uint32_t)115200 / 10;
    if (sr >= byte_rate) // lidt unødvendigt. Men HVIS vi skulle ændre til en lavere BAUD end 115200 eller højere sr end 10.000 så kunne vi få en bug uden.
    {
        return 0; //så ved vi da der er sket en fejl. 
    }

    uint32_t datapak = (uint32_t)7 * sr; //parentesen fordi sr kun er uint16, så vi skal bruge at 7 er uint32 for ikk at risikere at den wrapper

    if (rl <= (datapak / (byte_rate - sr)))
    {
        if ((datapak % (byte_rate - sr)) == 0){
            rl = (datapak / (byte_rate - sr)); 
        }
        else 
        {
            rl = (datapak / (byte_rate - sr)) + 1;
        }
        rl = rl * 1.1;
    }

    

    if (rl < 10) rl = 10;
    if (rl > MAX_RECORD_LENGTH) return MAX_RECORD_LENGTH;

    return rl;
}

ISR(ADC_vect)
{
    if (bode_mode) {
        bode_sample = ADCH;
        ADCSRA |= (1 << ADSC);
        bode_sample_ready = 1;
        return;
    }

    active_buffer[buffer_index++] = ADCH;

    if (buffer_index >= record_length) {
        buffer_index = 0;

        if (!buffer_ready) {
            volatile uint8_t *tmp = active_buffer;
            active_buffer         = ready_buffer;
            ready_buffer          = tmp;
            buffer_ready          = 1;
        }
    }
}

void adc_init(uint16_t sample_rate, uint16_t rec_length)
{
    sample_rate = clamp_sample_rate(sample_rate);
    rec_length  = clamp_record_length(rec_length, sample_rate);

    current_sample_rate = sample_rate;
    record_length = rec_length;

    // ekstern AREF, venstrejustering (8-bit fra ADCH), ADC0
    ADMUX = (0 << REFS1) | (0 << REFS0) | (1 << ADLAR) | (0 << MUX0);

    // prescaler 16, ADC enable, ADC interrupt enable
    ADCSRA = (1 << ADEN) | (1 << ADIE)
           | (1 << ADPS2) | (0 << ADPS1) | (0 << ADPS0);

    timer_init(sample_rate);

    sei();
}

void adc_set_params(uint16_t sample_rate, uint16_t rec_length)
{
    sample_rate = clamp_sample_rate(sample_rate);
    rec_length  = clamp_record_length(rec_length, sample_rate);

    cli();
    current_sample_rate = sample_rate;
    record_length = rec_length;
    buffer_index  = 0;
    buffer_ready  = 0;
    timer_set_samplerate(sample_rate);
    sei();
}