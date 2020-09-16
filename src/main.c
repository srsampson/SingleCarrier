#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <complex.h>
#include <time.h>
#include <liquid/liquid.h>

#include "qpsk_internal.h"

/*
 * QPSK Quadrant bit-pair values - Gray Coded
 */
const complex float constellation[] = {
    1.0f + 0.0f * I,
    0.0f + 1.0f * I,
    0.0f - 1.0f * I,
    -1.0f + 0.0f * I
};

/*
 * These pilots are compatible with Octave version
 */
const int8_t pilotvalues[] = {
    -1, -1, 1, 1, -1, -1, -1, 1,
    -1, 1, -1, 1, 1, 1, 1, 1,
    1, 1, 1, -1, -1, 1, -1, 1,
    -1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, -1, 1, 1, 1, 1,
    1, -1, -1, -1, -1, -1, -1, 1,
    -1, 1, -1, 1, -1, -1, 1, -1,
    1, 1, 1, 1, -1, 1, -1, 1
};

static float cnormf(complex float val) {
    float realf = crealf(val);
    float imagf = cimagf(val);

    return realf * realf + imagf * imagf;
}

/*
 * Gray coded QPSK modulation function
 */
complex float qpsk_mod(int *bits) {
    return constellation[(bits[1] << 1) | bits[0]];
}

/*
 * Gray coded QPSK demodulation function
 *
 * 01 | 00
 * ---+---
 * 11 | 10
 */
void qpsk_demod(complex float symbol, int *bits) {
    complex float rotate = symbol * cmplx(ROT45);

    bits[0] = crealf(rotate) < 0.0f;
    bits[1] = cimagf(rotate) < 0.0f;
}

/*
 * Convert frequency sample into time domain
 */
static void idft(struct QPSK *qpsk, complex float *result, complex float *vector) {
    result[0] = vector[0] * qpsk->inv_m;

    for (int row = 1; row < qpsk->m; row++) {
        complex float c = cmplx(qpsk->doc * row);
        result[row] = (c * vector[0]) * qpsk->inv_m;
    }
}
/*
 * Convert time domain into frequency sample
 */
static void dft(struct QPSK *qpsk, complex float *result, complex float *vector) {
    result[0] = vector[0];

    complex float c = cmplxconj(qpsk->doc);
    complex float delta = c;

    for (int row = 1; row < qpsk->m; row++) {
        result[0] += (vector[row] * c);
        c *= delta;
    }
}

static complex float vector_sum(complex float *a, int num_elements) {
    complex float sum = 0.0f;

    for (int i = 0; i < num_elements; i++) {
        sum += a[i];
    }

    return sum;
}

int main(int argc, char** argv) {
    srand(time(0));
    
    struct QPSK *qpsk = (struct QPSK *) malloc(sizeof (struct QPSK));

    qpsk->ns = 8; /* Number of Symbol frames */
    qpsk->bps = 2; /* Bits per Symbol */
    qpsk->ts = 1.0f / 1600.0f;
    qpsk->centre = 1600.0f; /* Centre Audio Frequency */
    qpsk->fs = 8000.0f; /* Sample Frequency */
    qpsk->ntxtbits = 4;
    qpsk->ftwindowwidth = 11;
    qpsk->timing_mx_thresh = 0.30f;
    qpsk->rs = (1.0f / qpsk->ts); /* Modulation Symbol Rate 1600 baud */
    qpsk->m = (int) (qpsk->fs / qpsk->rs); /* 5 */
    qpsk->inv_m = (1.0f / (float) qpsk->m);
    qpsk->doc = (TAU / (qpsk->fs / qpsk->rs));  // @ 1.256636

    /* create the QPSK pilot time-domain waveform */
    

    complex float temp[qpsk->m];
    complex float pilot, data;
    int bpsk_val, qpsk_val;
    
    FILE *fout = fopen("/tmp/spectrum.raw", "wb");

    for (int k = 0; k < 5000; k++) {
        // 31 BPSK pilots
        for (int i = 0; i < 31; i++) {
            bpsk_val = (pilotvalues[i] == 1) ? 0 : 3;
            pilot = constellation[bpsk_val];
        
            idft(qpsk, temp, &pilot);

            for (int j = 0; j < qpsk->m; j++) {
                uint16_t val = (uint16_t) (crealf(temp[j]) * 50000.0f);
            
                fwrite(&val, sizeof (uint16_t), 1, fout);
            }
        }
        // 31 QPSK
        for (int i = 0; i < 31; i++) {
            qpsk_val = rand() % 4;
            data = constellation[qpsk_val];
        
            idft(qpsk, temp, &data);

            for (int j = 0; j < qpsk->m; j++) {
                uint16_t val = (uint16_t) (crealf(temp[j]) * 50000.0f);
            
                fwrite(&val, sizeof (uint16_t), 1, fout);
            }
        }
    }

    fclose(fout);
    
    free(qpsk);    

    return (EXIT_SUCCESS);
}
