#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <complex.h>
#include <time.h>

#include "qpsk_internal.h"

static complex float osc_table[OSC_TABLE_SIZE];
static int osc_table_offset;

/*
 * QPSK Quadrant bit-pair values - Gray Coded
 */
static const complex float constellation[] = {
    1.0f + 0.0f * I,
    0.0f + 1.0f * I,
    0.0f - 1.0f * I,
    -1.0f + 0.0f * I
};

/*
 * These pilots are compatible with Octave version
 */
static const int8_t pilotvalues[] = {
    -1, -1, 1, 1, -1, -1, -1, 1,
    -1, 1, -1, 1, 1, 1, 1, 1,
    1, 1, 1, -1, -1, 1, -1, 1,
    -1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, -1, 1, 1, 1, 1,
    1, -1, -1, -1, -1, -1, -1, 1,
    -1, 1, -1, 1, -1, -1, 1, -1,
    1, 1, 1, 1, -1, 1, -1, 1
};

/*
 * Created with:
 * hs = gen_rn_coeffs(.35, 0.000125, 1600, 8, 5);
 */
static const float rrccoeff[] = {
    0.00265568,
    0.00287612,
    0.00063112,
    -0.00295307,
    -0.00535752,
    -0.00409702,
    0.00147671,
    0.00908714,
    0.01405700,
    0.01154864,
    -0.00037972,
    -0.01847733,
    -0.03424136,
    -0.03663832,
    -0.01677543,
    0.02744741,
    0.08895061,
    0.15277814,
    0.20091748,
    //
    0.21881953,
    //
    0.20091748,
    0.15277814,
    0.08895061,
    0.02744741,
    -0.01677543,
    -0.03663832,
    -0.03424136,
    -0.01847733,
    -0.00037972,
    0.01154864,
    0.01405700,
    0.00908714,
    0.00147671,
    -0.00409702,
    -0.00535752,
    -0.00295307,
    0.00063112,
    0.00287612,
    0.00265568
};

#define RRCLEN 39

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
static void idft(struct QPSK *qpsk, complex float *result, complex float value) {
    result[0] = value * qpsk->inv_m;

    for (int row = 1; row < qpsk->m; row++) {
        result[row] = (cmplx(.1f * qpsk->doc * row) * value) * qpsk->inv_m;
        
        // the .1 cleaned-up the spectrum a bit ??
    }
}

/*
 * Convert time domain into frequency sample
 */
static void dft(struct QPSK *qpsk, complex float result, complex float *value) {
    result = value[0];

    complex float c = cmplxconj(qpsk->doc);
    complex float delta = c;

    for (int row = 1; row < qpsk->m; row++) {
        result += (value[row] * c);
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

static void fir(float in[], float out[], int points) {
    float delay[RRCLEN] = {0.0f};
    int i, j;

    for (i = 0; i < points; i++) {
        for (j = RRCLEN; j > 1; j--) {
            delay[j-1] = delay[j-2];
        }
  
        delay[0] = in[i];

        float y = 0.0f;
        
        for (j = 0; j < RRCLEN; j++) {
            y += rrccoeff[j] * delay[j];
        }
        
        out[i] = y;
    }
}

int main(int argc, char** argv) {
    srand(time(0));

    struct QPSK *qpsk = (struct QPSK *) malloc(sizeof (struct QPSK));

    float baud = 1600.0f;
    
    qpsk->fs = 8000.0f; /* Sample Frequency */
    qpsk->centre = 1200.0;
    
    osc_table_offset = 0;
        
    for (int i = 0; i < OSC_TABLE_SIZE; i++) {
        osc_table[i] = cmplx(TAU * qpsk->centre * ((float) i / qpsk->fs));
    }
    
    qpsk->ns = 8; /* Number of Symbol frames */
    qpsk->bps = 2; /* Bits per Symbol */
    qpsk->ts = 1.0f / baud;
    qpsk->rs = (1.0f / qpsk->ts); /* Symbol Rate */

    qpsk->fs = 8000.0f; /* Sample Frequency */
    qpsk->doc = (TAU / (qpsk->fs / qpsk->rs)); // @ 1.256636
    qpsk->m = (int) (qpsk->fs / qpsk->rs); /* 5 */
    qpsk->inv_m = (1.0f / (float) qpsk->m);

    qpsk->ntxtbits = 4;
    qpsk->ftwindowwidth = 11;
    qpsk->timing_mx_thresh = 0.30f;

    /* create the QPSK pilot time-domain waveform */

    float frame[160000] = { 0.0f };
    complex float temp[qpsk->m];
    complex float pilot, data;
    int bpsk_val, qpsk_val;

    FILE *fout = fopen("/tmp/spectrum.raw", "wb");

    int next = 0;

    for (int k = 0; k < 500; k++) {
        // 31 BPSK pilots
        for (int i = 0; i < 31; i++) {
            bpsk_val = (pilotvalues[i] == 1) ? 0 : 3;
            pilot = constellation[bpsk_val];

            idft(qpsk, temp, pilot);

            for (int j = 0; j < qpsk->m; j++) {
                frame[next++] =  crealf(temp[j]);
            }
        }
        // 31 QPSK
        for (int i = 0; i < 31; i++) {
            qpsk_val = rand() % 4;
            data = constellation[qpsk_val];

            idft(qpsk, temp, data);

            for (int j = 0; j < qpsk->m; j++) {
                frame[next++] = crealf(temp[j]);
            }
        }
    }

    float out[next];
    
    fir(&frame[0], &out[0], next);
    
    for (int i = 0; i < next; i++) {
        int16_t val = (int16_t) (crealf(osc_table[osc_table_offset] * out[i]) * 50000);
        osc_table_offset = (osc_table_offset + 1) % OSC_TABLE_SIZE;
        fwrite(&val, sizeof (int16_t), 1, fout);
    }

    free(qpsk);
    fclose(fout);

    return (EXIT_SUCCESS);
}
