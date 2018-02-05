//
//  fix_fft.h
//  fixedTest
//
//  Created by Adam on 2018-01-28.
//  Copyright © 2018 Adam. All rights reserved.
//

#ifndef fix_fft_h
#define fix_fft_h

#define FFT_Q7
#include <stdio.h>


#ifdef FFT_Q7
int16_t fix_fftr(int8_t f[], uint8_t m, uint8_t inverse);
uint16_t fix_fft(int8_t fr[], int8_t fi[], uint8_t m, uint8_t inverse);
void split(int8_t* X, int8_t* G, uint16_t N);
#else
int16_t fix_fftr(int16_t f[], uint8_t m, uint8_t inverse);
uint16_t fix_fft(int16_t fr[], int16_t fi[], uint8_t m, uint8_t inverse);
void split(int16_t* X, int16_t* G, uint16_t N);
#endif



#endif /* fix_fft_h */
