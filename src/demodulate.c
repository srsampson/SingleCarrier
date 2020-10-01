#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <complex.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "qpsk.h"

extern int16_t tx_samples;
extern int tx_sample_offset;

static FILE *fin;
static FILE *fout;

complex float osc_table[OSC_TABLE_SIZE];

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
const int pilotvalues[PILOT_SYMBOLS] = {
    -1, -1, 1, 1, -1, -1, -1, 1,
    -1, 1, -1, 1, 1, 1, 1, 1,
    1, 1, 1, -1, -1, 1, -1, 1,
    -1, 1, 1, 1, 1, 1, 1, 1, 1
};

float pilot_table[PILOT_SAMPLES];

/*
 * Created with:
 * hs = gen_rn_coeffs(.35, 0.000125, 1600, 8, 5);
 */
const float rrccoeff[] = {
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

int main(int argc, char **argv) {
    int bits[6400], i, j;
    int16_t frame[RX_SAMPLES_SIZE];
    
    for (int i = 0; i < OSC_TABLE_SIZE; i++) {
        osc_table[i] = cmplx(TAU * CENTER * ((float) i / FS));
    }

    for (i = 0; i < PILOT_SAMPLES; i += CYCLES) {
        for (j = 0; j < CYCLES; j++) {
            pilot_table[i+j] = (float) pilotvalues[i];  // -1 or 1
        }
    }

    fin = fopen("/tmp/spectrum-filtered.raw", "r");
    fout = fopen("/tmp/spectrum.raw", "wb");

    while (1) {
        size_t count = fread(frame, sizeof(int16_t), RX_SAMPLES_SIZE, fin);

        if (count != RX_SAMPLES_SIZE)
            break;

        receive_frame(frame, bits, fout);
    }
    
    fclose(fin);
    fclose(fout);
    
    return (EXIT_SUCCESS);
}

