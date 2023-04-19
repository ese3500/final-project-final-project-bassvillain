#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdlib.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdio.h>
#include "uart.h"

typedef struct ComplexNumber {
    float r;
    float c;
} complex;

volatile int flag = 1;
volatile unsigned int index = 0;
volatile unsigned int index2 = 0;
volatile complex* samples;
volatile complex* output;

#define SAMPLE_SIZE 256
#define BAUD_RATE 9600
#define BAUD_PRESCALER ( (F_CPU / (BAUD_RATE*16UL)) - 1 )

void dft(volatile complex *sample_list, volatile complex *out, int high) {
    if (high <= 1) {
        return;
    } else {
        complex *even = out;
        complex *odd = out + (high / 2);
        for (int i = 0; i < high / 2; i++) {
            even[i] = sample_list[2 * i];
            odd[i] = sample_list[2 * i + 1];
        }
        dft(even, sample_list, high / 2);
        dft(odd, sample_list, high / 2);
        double pr;
        double pc;
        double qr;
        double qc;
        double rf;
        double cf;
        double qor;
        double qoc;
        for (int i = 0; i < high / 2; i++) {
            pr = even[i].r;
            pc = even[i].c;
            qr = odd[i].r;
            qc = odd[i].c;
            rf = cos((2 * M_PI * i) / ((double)high));
            cf = -sin((2 * M_PI * i) / ((double)high));
            qor = (rf * qr) - (cf * qc);
            qoc = (rf * qc) + (cf * qr);
            sample_list[i] = (complex){pr + qor, pc + qoc};
            sample_list[i + (high / 2)] = (complex){pr - qor, pc - qoc};
//            char buf[256];
//            sprintf(buf, "Sample: %f\n", sample_list[i].r);
//            sprintf(buf, "Sample: %f\n", sample_list[i + high/2].r);
//            UART_STRING(buf);
        }
    }
}

void clock_init(void) {
    cli();

    //OC0A/OC0B disconnected
    TCCR0A &= ~(1<<COM0A1);
    TCCR0A &= ~(1<<COM0A0);
    TCCR0A &= ~(1<<COM0B1);
    TCCR0A &= ~(1<<COM0B0);
    //set clock to Normal Mode
    TCCR0A &= ~(1<<WGM00);
    TCCR0A &= ~(1<<WGM01);
    TCCR0B &= ~(1<<WGM02);
    // turn clock on without prescaling
    TCCR0B &= ~(1<<CS02);
    TCCR0B |= (1<<CS00);
    TCCR0B &= ~(1<<CS01);

    //activate overflow interrupt
    TIMSK0 |= (1<<TOIE0);
    sei();
}

void switch_init(void) {
    cli();
    EICRA |= (1<<ISC01);
    EICRA |= (1<<ISC00);
    EIMSK |= (1<<INT0);
    sei();
}

void adc_init(void) {
    //setup ADC
    cli();
    PRR0 &= ~(1<<PRADC);

    //select Vref
    ADMUX |= (1<<REFS0);
    ADMUX &= ~(1<<REFS1);

    //set ADC Clock div by 128
    ADCSRA |= (1<<ADPS0);
    ADCSRA |= (1<<ADPS1);
    ADCSRA |= (1<<ADPS2);

    //select channel 0 or 1 based on D4 read
    ADMUX &= ~(1<<MUX0);
    ADMUX &= ~(1<<MUX1);
    ADMUX &= ~(1<<MUX2);
    ADMUX &= ~(1<<MUX3);

    //Set to free running
    ADCSRA |= (1<<ADATE);
    ADCSRB &= ~(1<<ADTS0);
    ADCSRB &= ~(1<<ADTS1);
    ADCSRB &= ~(1<<ADTS2);

    //disable input buffer on ADC pin
    DIDR0 |= (1<<ADC0D);

    //enable ADC
    ADCSRA |= (1<<ADEN);
    ADCSRA |= (1<<ADIE);

    ADCSRA |= (1<<ADSC);
    sei();
}

void init_sample(void) {
    samples = (complex*) malloc(sizeof(complex)*SAMPLE_SIZE);
    output = (complex*) malloc(sizeof(complex)*SAMPLE_SIZE);
}

void sample_freq(void) {
    if (index >= SAMPLE_SIZE) { flag = 0; index = 0; }
    index++;
    samples[index].r = 4.0;
    samples[index].c = 0;
}

ISR(ADC_vect) {
    if (flag) {
        sample_freq();
    }
    ADCSRA |= (1<<ADIF);
}

//ISR(PCINT0_vect) {
//    if (!flag) {
//        complex *fft = dft(samples, 0, SAMPLE_SIZE);
//        double max = -1;
//        int imax = 0;
//        for (int i = 0; i < SAMPLE_SIZE; i++) {
//            double curr = fft[i].r;
//            if (curr - max > 1) {
//                max = curr;
//                imax = i;
//            }
//        }
//        // printf("imax: %d\n", imax);
//        char buffer[256];
//        sprintf(buffer, "Approximate frequency: %f\n", (62500.0 / SAMPLE_SIZE) * imax);
//        UART_STRING(buffer);
//    }
//    EIFR |= (1<<INTF0);
//}



int main(void) {
    DDRD |= (1<<DDD3);
    PORTD |= (1<<PORTD3);
    char buf[256];
    init_sample();
    adc_init();
    switch_init();
    UART_INIT(BAUD_PRESCALER);
    while(flag);
//    for (int i = 0; i < SAMPLE_SIZE; i++) {
//        samples[i].r = (sin((2 * M_PI * 440 * i) / 4800));
//        samples[i].c = 0;
//        // printf("%d\n", sample_list[i]);
//    }
    dft(samples, output, SAMPLE_SIZE);
    float max = -1;
    int imax = 0;
    for (int i = 0; i < SAMPLE_SIZE; i++) {
        float curr = samples[i].r;
        if (curr - max > 1) {
            max = curr;
            imax = i;
        }
    }
    // printf("imax: %d\n", imax);
    sprintf(buf, "Approximate frequency: %f\n", (4800.0 / SAMPLE_SIZE) * imax);
    UART_STRING(buf);
}