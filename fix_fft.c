
#include "fix_fft.h"

/* fix_fft.c - Fixed-point in-place Fast Fourier Transform  */
/*
 All data are fixed-point int8_t integers, in which -128
 to +127 represent -1.0 to +1.0 respectively. Integer
 arithmetic is used for speed, instead of the more natural
 floating-point.
 For the forward FFT (time -> freq), fixed scaling is
 performed to prevent arithmetic overflow, and to map a 0dB
 sine/cosine wave (i.e. amplitude = 32767) to two -6dB freq
 coefficients. The return value is always 0.
 For the inverse FFT (freq -> time), fixed scaling cannot be
 done, as two 0dB coefficients would sum to a peak amplitude
 of 64K, overflowing the 32k range of the fixed-point integers.
 Thus, the fix_fft() routine performs variable scaling, and
 returns a value which is the number of bits LEFT by which
 the output must be shifted to get the actual amplitude
 (i.e. if fix_fft() returns 3, each value of fr[] and fi[]
 must be multiplied by 8 (2**3) for proper scaling.
 Clearly, this cannot be done within fixed-point short
 integers. In practice, if the result is to be used as a
 filter, the scale_shift can usually be ignored, as the
 result will be approximately correctly normalized as is.
 Written by:  Tom Roberts  11/8/89
 Made portable:  Malcolm Slaney 12/15/94 malcolm@interval.com
 Enhanced:  Dimitrios P. Bouras  14 Jun 2006 dbouras@ieee.org
 8-bit ported, split function added for real fft: Adam Myrén 26 Jan 2018
 */

#define N_WAVE      256    /* full length of Sinewave[], this is the max. nr of points of the FFT*/
#define LOG2_N_WAVE 8      /* log2(N_WAVE) */

#include <stdint.h>
#include <math.h>
/*
 Henceforth "short" implies 16-bit word. If this is not
 the case in your architecture, please replace "short"
 with a type definition which *is* a 16-bit word.
 */

/*
 Since we only use 3/4 of N_WAVE, we define only
 this many samples, in order to conserve data space.
 */
#ifdef FFT_Q7
int8_t Sinewave[N_WAVE-N_WAVE/4] = {
    0,    3,    6,    9,    12,   15,   18,   21,
    24,   28,   31,   34,   37,   40,   43,   46,
    48,   51,   54,   57,   60,   63,   65,   68,
    71,   73,   76,   78,   81,   83,   85,   88,
    90,   92,   94,   96,   98,   100,  102,  104,
    106,  108,  109,  111,  112,  114,  115,  117,
    118,  119,  120,  121,  122,  123,  124,  124,
    125,  126,  126,  127,  127,  127,  127,  127,
    127,  127,  127,  127,  127,  127,  126,  126,
    125,  124,  124,  123,  122,  121,  120,  119,
    118,  117,  115,  114,  112,  111,  109,  108,
    106,  104,  102,  100,  98,   96,   94,   92,
    90,   88,   85,   83,   81,   78,   76,   73,
    71,   68,   65,   63,   60,   57,   54,   51,
    48,   46,   43,   40,   37,   34,   31,   28,
    24,   21,   18,   15,   12,   9,    6,    3,
    0,    -3,   -6,   -9,   -12,  -15,  -18,  -21,
    -24,  -28,  -31,  -34,  -37,  -40,  -43,  -46,
    -48,  -51,  -54,  -57,  -60,  -63,  -65,  -68,
    -71,  -73,  -76,  -78,  -81,  -83,  -85,  -88,
    -90,  -92,  -94,  -96,  -98,  -100, -102, -104,
    -106, -108, -109, -111, -112, -114, -115, -117,
    -118, -119, -120, -121, -122, -123, -124, -124,
    -125, -126, -126, -127, -127, -127, -127, -127,
    /*
    -127, -127, -127, -127, -127, -127, -126, -126,
    -125, -124, -124, -123, -122, -121, -120, -119,
    -118, -117, -115, -114, -112, -111, -109, -108,
    -106, -104, -102, -100, -98,  -96,  -94,  -92,
    -90,  -88,  -85,  -83,  -81,  -78,  -76,  -73,
    -71,  -68,  -65,  -63,  -60,  -57,  -54,  -51,
    -48,  -46,  -43,  -40,  -37,  -34,  -31,  -28,
    -24,  -21,  -18,  -15,  -12,  -9,   -6,   -3, 
     */
};
#else
int16_t Sinewave[N_WAVE-N_WAVE/4] = {
    0,      808,    1616,   2423,   3228,   4032,   4833,   5631,
    6426,   7216,   8003,   8784,   9560,   10330,  11094,  11852,
    12602,  13344,  14078,  14804,  15520,  16228,  16925,  17612,
    18288,  18953,  19607,  20249,  20878,  21495,  22098,  22689,
    23265,  23827,  24375,  24907,  25425,  25927,  26414,  26884,
    27338,  27775,  28196,  28599,  28985,  29353,  29703,  30036,
    30350,  30645,  30922,  31180,  31419,  31639,  31839,  32021,
    32183,  32325,  32447,  32550,  32633,  32696,  32739,  32763,
    32766,  32749,  32713,  32656,  32580,  32484,  32368,  32232,
    32077,  31902,  31708,  31494,  31262,  31010,  30739,  30450,
    30142,  29816,  29472,  29109,  28730,  28332,  27917,  27486,
    27037,  26572,  26091,  25594,  25082,  24554,  24011,  23454,
    22882,  22297,  21697,  21085,  20460,  19822,  19173,  18511,
    17839,  17155,  16461,  15757,  15044,  14321,  13590,  12850,
    12102,  11348,  10586,  9818,   9043,   8264,   7479,   6690,
    5896,   5099,   4299,   3497,   2692,   1885,   1078,   269,
    -539,   -1347,  -2154,  -2960,  -3764,  -4566,  -5365,  -6161,
    -6953,  -7741,  -8524,  -9302,  -10074, -10840, -11600, -12352,
    -13097, -13834, -14563, -15283, -15993, -16694, -17384, -18064,
    -18733, -19391, -20036, -20670, -21291, -21899, -22493, -23074,
    -23641, -24194, -24732, -25254, -25762, -26253, -26729, -27188,
    -27631, -28057, -28466, -28858, -29232, -29589, -29927, -30247,
    -30549, -30832, -31096, -31341, -31568, -31775, -31963, -32131,
    -32280, -32409, -32518, -32608, -32677, -32727, -32757, -32767,
};

#endif



/*
 FIX_MPY() - fixed-point multiplication & scaling.
 Substitute inline assembly for hardware-specific
 optimization suited to a particluar DSP processor.
 Scaling ensures that result remains 16-bit.
 */
#ifdef FFT_Q7
int8_t fixmul(int8_t a, int8_t b)
{
    /* shift right one less bit (i.e. 15-1) */
    int16_t c = ((int16_t)a * (int16_t)b) >> 6;
    /* last bit shifted out = rounding-bit */
    b = c & 0x01;
    /* last shift + rounding bit */
    a = (c >> 1) + b;
    return a;
}
#else
int16_t fixmul(int16_t a, int16_t b)
{
    /* shift right one less bit (i.e. 15-1) */
    int32_t c = ((int32_t)a * (int32_t)b) >> 14;
    /* last bit shifted out = rounding-bit */
    b = c & 0x01;
    /* last shift + rounding bit */
    a = (c >> 1) + b;
    return a;
}
#endif
/*
 fix_fft() - perform forward/inverse fast Fourier transform.
 fr[n],fi[n] are real and imaginary arrays, both INPUT AND
 RESULT (in-place FFT), with 0 <= n < 2**m; set inverse to
 0 for forward transform (FFT), or 1 for iFFT.
 */
#ifdef FFT_Q7
uint16_t fix_fft(int8_t fr[], int8_t fi[], uint8_t m, uint8_t inverse)
#else
uint16_t fix_fft(int16_t fr[], int16_t fi[], uint8_t m, uint8_t inverse)
#endif
{
    int16_t mr, nn, i, j, l, k, istep, n, scale, shift;
#ifdef FFT_Q7
    int8_t qr, qi, tr, ti, wr, wi;
#else
    int16_t qr, qi, tr, ti, wr, wi;
#endif
    
    n = 1 << m;
    
    /* max FFT size = N_WAVE */
    if (n > N_WAVE)
        return -1;
    
    mr = 0;
    nn = n - 1;
    scale = 0;
    
    /* decimation in time - re-order data */
    for (m=1; m<=nn; ++m) {
        l = n;
        do {
            l >>= 1;
        } while (mr+l > nn);
        mr = (mr & (l-1)) + l;
        
        if (mr <= m)
            continue;
        tr = fr[m];
        fr[m] = fr[mr];
        fr[mr] = tr;
        ti = fi[m];
        fi[m] = fi[mr];
        fi[mr] = ti;
    }
    
    l = 1;
    k = LOG2_N_WAVE-1;
    while (l < n) {
        if (inverse) {
            /* variable scaling, depending upon data */
            shift = 0;
            for (i=0; i<n; ++i) {
                j = fr[i];
                if (j < 0)
                    j = -j;
                m = fi[i];
                if (m < 0)
                    m = -m;
#ifdef FFT_Q7
                if (j > 63 || m > 63) {
#else
                if (j > 16383 || m > 16383) {
#endif
                    shift = 1;
                    break;
                }
            }
            if (shift)
                ++scale;
        } else {
            /*
             fixed scaling, for proper normalization --
             there will be log2(n) passes, so this results
             in an overall factor of 1/n, distributed to
             maximize arithmetic accuracy.
             */
            shift = 1;
        }
        /*
         it may not be obvious, but the shift will be
         performed on each data point exactly once,
         during this pass.
         */
        istep = l << 1;
        for (m=0; m<l; ++m) {
            j = m << k;
            /* 0 <= j < N_WAVE/2 */
            wr =  Sinewave[j+N_WAVE/4];
            wi = -Sinewave[j];
            if (inverse)
                wi = -wi;
            if (shift) {
                wr >>= 1;
                wi >>= 1;
            }
            for (i=m; i<n; i+=istep) {
                j = i + l;
                tr = fixmul(wr,fr[j]) - fixmul(wi,fi[j]);
                ti = fixmul(wr,fi[j]) + fixmul(wi,fr[j]);
                qr = fr[i];
                qi = fi[i];
                if (shift) {
                    qr >>= 1;
                    qi >>= 1;
                }
                fr[j] = qr - tr;
                fi[j] = qi - ti;
                fr[i] = qr + tr;
                fi[i] = qi + ti;
            }
        }
        --k;
        l = istep;
    }
    return scale;
}

/*
 fix_fftr() - forward/inverse FFT on array of real numbers.
 Real FFT/iFFT using half-size complex FFT by distributing
 even/odd samples into real/imaginary arrays respectively.
 In order to save data space (i.e. to avoid two arrays, one
 for real, one for imaginary samples), we proceed in the
 following two steps: a) samples are rearranged in the real
 array so that all even samples are in places 0-(N/2-1) and
 all imaginary samples in places (N/2)-(N-1), and b) fix_fft
 is called with fr and fi pointing to index 0 and index N/2
 respectively in the original array. The above guarantees
 that fix_fft "sees" consecutive real samples as alternating
 real and imaginary samples in the complex array.
 */
#ifdef FFT_Q7
int16_t fix_fftr(int8_t f[], uint8_t m, uint8_t inverse)
#else
int16_t fix_fftr(int16_t f[], uint8_t m, uint8_t inverse)
#endif
{
    int16_t i, N = 1<<(m-1), scale = 0;
#ifdef FFT_Q7
    int8_t tt, *fr=f, *fi=&f[N];
#else
    int16_t tt, *fr=f, *fi=&f[N];
#endif
    
    if (inverse)
        scale = fix_fft(fi, fr, m-1, inverse);
    /*
    for (i=1; i<N; i+=2) {
        tt = f[N+i-1];
        f[N+i-1] = f[i];
        f[i] = tt;
    }
    */
    
    int8_t frt[N];
    int8_t fit[N];
    for(i=0; i<N; i++) {
        /*
        tt = f[i];
        fr[i] = f[2*i];
        fi[i] = f[2*i+1];
         */
        
        frt[i] = f[2*i];
        fit[i] = f[2*i+1];
        
    }
    
    for(i=0; i<N; i++) {
        fr[i] = frt[i];
        fi[i] = fit[i];
    }
    
    if (! inverse)
        scale = fix_fft(fi, fr, m-1, inverse);
    return scale;
}

#ifdef FFT_Q7
void split(int8_t* X, int8_t* G, uint16_t N) {
    int8_t *Xr=X, *Xi=&X[N/2], *Gr=G, *Gi=&G[N/2];
    for(uint8_t i=0; i<N/2; i++) {
        int8_t Ai = (-Sinewave[2*i+N_WAVE/4])>>1;
        int8_t Ar = (127 - Sinewave[2*i])>>1;
        int8_t Bi = (Sinewave[2*i+N_WAVE/4])>>1;
        int8_t Br = (127 + Sinewave[2*i])>>1;

        Gr[i] = ((Xr[i]*Ar)>>7) - ((Xi[i]*Ai)>>7) + ((Xr[(N/2)-i]*Br)>>7) + ((Xi[(N/2)-i]*Bi)>>7);
        Gi[i] = ((Xi[i]*Ar)>>7) + ((Xr[i]*Ai)>>7)  + ((Xr[(N/2)-i]*Bi)>>7) - ((Xi[(N/2)-i]*Br)>>7);
    }
}
#else
void split(int16_t* X, int16_t* G, uint16_t N) {
    int16_t *Xr=X, *Xi=&X[N/2], *Gr=G, *Gi=&G[N/2];
    for(uint8_t i=0; i<N/2; i++) {
        int8_t Ai = (-Sinewave[2*i+N_WAVE/4])>>1;
        int8_t Ar = (32767 - Sinewave[2*i])>>1;
        int8_t Bi = (Sinewave[2*i+N_WAVE/4])>>1;
        int8_t Br = (32767 + Sinewave[2*i])>>1;
        
        Gr[i] = ((Xr[i]*Ar)>>15) - ((Xi[i]*Ai)>>15) + ((Xr[(N/2)-i]*Br)>>15) + ((Xi[(N/2)-i]*Bi)>>15);
        Gi[i] = ((Xi[i]*Ar)>>15) + ((Xr[i]*Ai)>>15)  + ((Xr[(N/2)-i]*Bi)>>15) - ((Xi[(N/2)-i]*Br)>>15);
    }
}
#endif
