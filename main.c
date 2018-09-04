/*
ATmega8, 48, 88, 168, 328

    /Reset PC6|1   28|PC5		SCL
Nokia.SCL  PD0|2   27|PC4		SDA
Nokia.SDIN PD1|3   26|PC3     
Nokia.DC   PD2|4   25|PC2     
Nokia.SCE  PD3|5   24|PC1
Nokia.RST  PD4|6   23|PC0		Signal input
           Vcc|7   22|Gnd
           Gnd|8   21|Aref
Xtal       PB6|9   20|AVcc
Xtal       PB7|10  19|PB5 SCK  
           PD5|11  18|PB4 MISO 
           PD6|12  17|PB3 MOSI 
           PD7|13  16|PB2      
           PB0|14  15|PB1      
*/

#define F_CPU 20000000UL
#define FFT_SIZE	128
#define M			7

#include <avr/io.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>


// display routines for the graphics LCD
#include "nokia5110.h"
// FFT routines
#include "fix_fft.h"


volatile int8_t* buff;
volatile int8_t adc_buff1[FFT_SIZE];
volatile int8_t adc_buff2[FFT_SIZE];

/*
volatile int16_t* buff;
volatile int16_t adc_buff1[FFT_SIZE];
volatile int16_t adc_buff2[FFT_SIZE];
*/
uint8_t buff_flag = 1;
uint8_t count = 0;
uint8_t mode = 0;
volatile uint8_t buff_ready = 0;


typedef struct {
	uint8_t bar;
	uint8_t max;
}Bars;


ISR(ADC_vect) {

	uint8_t signal = ADCH;
	
	*(buff+count) = signal-128;
	count++;
	if (count >= FFT_SIZE) {
		PORTB ^= (1 << PB1);
		if(buff_flag) { //adc_buff2 ready
			buff = adc_buff1;
			buff_flag = 0;
		}
		else {			//adc_buff1 ready
			buff = adc_buff2;
			buff_flag = 1;
		}
		if(buff_ready != 0) {
			//PORTB |= (1 << PB1); // Debug
		}	
		buff_ready = 1;
		count = 0; 
	}
}

void adc_init(void)
{
	// ADC init
	//  reference voltage: supply AVCC
	//  channel ADC0
	//  clock: f_cpu/d 
	//  Left-aligned result
	ADMUX  = (0 << REFS1) | (1 << REFS0) | (1 << ADLAR)
		   | (0 << MUX3)  | (0 << MUX2)  | (0 << MUX1) | (0 << MUX0);
	ADCSRA = (1 << ADEN)  | (1 << ADSC)  | (1 << ADATE) | (1 << ADIE)
		   | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
	ADCSRB = (0 << ADTS2) | (0 << ADTS1) | (0 << ADTS0); // Free running mode
}

void display_init(void)
{
	NOKIA_init(0);
	NOKIA_LED_ENABLE();
	NOKIA_setVop(0x3A);
}

// Log table for fast logarithmic calculations on 8-bit numbers
uint8_t logT[256] =
{
	1,  6,  10, 12, 14, 16, 17, 18,
	19, 20, 21, 22, 22, 23, 24, 24,
	25, 25, 26, 26, 26, 27, 27, 28,
	28, 28, 29, 29, 29, 30, 30, 30,
	30, 31, 31, 31, 31, 32, 32, 32,
	32, 32, 33, 33, 33, 33, 33, 34,
	34, 34, 34, 34, 34, 35, 35, 35,
	35, 35, 35, 36, 36, 36, 36, 36,
	36, 36, 37, 37, 37, 37, 37, 37,
	37, 37, 38, 38, 38, 38, 38, 38,
	38, 38, 38, 38, 39, 39, 39, 39,
	39, 39, 39, 39, 39, 39, 40, 40,
	40, 40, 40, 40, 40, 40, 40, 40,
	40, 41, 41, 41, 41, 41, 41, 41,
	41, 41, 41, 41, 41, 41, 42, 42,
	42, 42, 42, 42, 42, 42, 42, 42,
	42, 42, 42, 42, 42, 43, 43, 43,
	43, 43, 43, 43, 43, 43, 43, 43,
	43, 43, 43, 43, 43, 44, 44, 44,
	44, 44, 44, 44, 44, 44, 44, 44,
	44, 44, 44, 44, 44, 44, 44, 45,
	45, 45, 45, 45, 45, 45, 45, 45,
	45, 45, 45, 45, 45, 45, 45, 45,
	45, 45, 45, 45, 46, 46, 46, 46,
	46, 46, 46, 46, 46, 46, 46, 46,
	46, 46, 46, 46, 46, 46, 46, 46,
	46, 46, 46, 47, 47, 47, 47, 47,
	47, 47, 47, 47, 47, 47, 47, 47,
	47, 47, 47, 47, 47, 47, 47, 47,
	47, 47, 47, 47, 47, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48,
	48, 48, 48, 48, 48, 48, 48, 48,
};

void display(Bars *bars, uint8_t mode, char *mic, char *aux) {
	NOKIA_clearbuffer();
	
	if(mode == 0) {
		for (uint8_t k=0; k<strlen(mic); k++) {
			NOKIA_putchar(77, k*8, mic[k], 0);
		}
	}
	else if(mode == 1) {
		for (uint8_t k=0; k<strlen(aux); k++) {
			NOKIA_putchar(77, k*8, aux[k], 0);
		}
	}
	
	for(uint8_t i=1; i<(FFT_SIZE/2); i++){
		for(uint8_t j=0; j<=(logT[bars[i].bar]); j++){
			NOKIA_setpixel(i, 48-j);
		}
		NOKIA_setpixel(i, 47-(bars[i].max));
		NOKIA_setpixel(i, 47-(bars[i].max) + 1);
	}
	
	NOKIA_update();
}



int main(void) {
	uint8_t button_flag = 0;
	uint8_t switch_flag = 0;
	buff = adc_buff1;
	Bars bars[FFT_SIZE/2];
	int8_t img_fft[FFT_SIZE];
	char mic_string[] = "mic";
	char aux_string[] = "aux";
	display_init();
	adc_init();	
	uint8_t clear = 0;
	DDRB |= (1 << PB0) | (1 << PB1);
	DDRD &= ~(1 << PD1);
	PORTD |= (1 << PD1); //Pull-up
	sei();				// Global interrupt flag
	while (1) {
		
		while (!buff_ready); 
		PORTB |= (1 << PB0);
		if (buff_flag) {
			fix_fft(adc_buff1, img_fft, M, 0);
			for(uint8_t i=0; i<(FFT_SIZE/2); i++){
				bars[i].bar = (((adc_buff1[i]*adc_buff1[i])>>5) + ((img_fft[i]*img_fft[i])>>5));
				if(bars[i].bar > bars[i].max) {
					bars[i].max = logT[bars[i].bar];
				}
				if(bars[i].max > 1) {
					bars[i].max -= 0;
				} else bars[i].max = 0;
			}
		} else if (!buff_flag) {
			fix_fft(adc_buff2, img_fft, M, 0);
			for(uint8_t i=0; i<(FFT_SIZE/2); i++){
				bars[i].bar = (((adc_buff2[i]*adc_buff2[i])>>5) + ((img_fft[i]*img_fft[i])>>5));
				if(bars[i].bar > bars[i].max) {
					bars[i].max = logT[bars[i].bar];
				}
				if(bars[i].max > 1) {
					bars[i].max -= 1;
				} else bars[i].max = 0;
			}
		}
		
		display(&bars, mode, mic_string, aux_string);

		if(!(PIND & (1<<PD1))) {
			_delay_ms(50);
			if(!(PIND & (1<<PD1))) {
				if(button_flag == 0) {
					mode++;
					switch_flag = 1;
					if (mode>1) { mode=0; }
				}
				button_flag = 1;
			}
			
		}
		else {
			button_flag = 0;
		}
		
		if (switch_flag == 1) {
			switch (mode) {
				case 0:
					ADMUX &= 0xf0;
					break;
				case 1:
					ADMUX &= 0xf0;
					ADMUX |= 0x01;
					break;

				default:
					break;
			}
			switch_flag=0;
		}
	
		//PORTB &= ~(1 << PB0); // Debug
		buff_ready = 0;
	}
}
