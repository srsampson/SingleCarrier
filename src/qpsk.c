#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <complex.h>
#include <string.h>
#include <time.h>

#include "qpsk_internal.h"

static complex float osc_table[OSC_TABLE_SIZE];
static complex float tx_filter[RRCLEN];

static int16_t tx_samples[TX_SAMPLES_SIZE];

static int osc_table_offset;
static int sample_offset;

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

static complex float vector_sum(complex float *a, int num_elements) {
    complex float sum = 0.0f;

    for (int i = 0; i < num_elements; i++) {
        sum += a[i];
    }

    return sum;
}

static void tx_symbol(struct QPSK *qpsk, complex float symbol, bool filtered) {
    complex float y;
    int i, j;

    for (i = 0; i < qpsk->m; i++) {
        if (filtered) {
            for (j = 0; j < RRCLEN - 1; j++) {
                tx_filter[j] = tx_filter[j + 1];
            }

            tx_filter[j] = symbol;

            y = 0.0f;

            for (j = 0; j < RRCLEN; j++) {
                y += tx_filter[j] * rrccoeff[j];
            }

            y *= osc_table[osc_table_offset];
            osc_table_offset = (osc_table_offset + 1) % OSC_TABLE_SIZE;

            tx_samples[sample_offset] = (int16_t) (crealf(y) * 16384.0f);
            sample_offset = (sample_offset + 1) % TX_SAMPLES_SIZE;
        } else {
            y = symbol * osc_table[osc_table_offset];
            osc_table_offset = (osc_table_offset + 1) % OSC_TABLE_SIZE;

            tx_samples[sample_offset] = (int16_t) (crealf(y) * 16384.0f);
            sample_offset = (sample_offset + 1) % TX_SAMPLES_SIZE;
        }
    }
}

/*
 * Zero out the FIR buffer
 */
static void flush_tx_filter() {
    for (int i = 0; i < RRCLEN; i++) {
        tx_filter[i] = 0.0f;
    }
}

int main(int argc, char** argv) {
    bool filtered;

    if (argc == 2) {
        if (strcmp("--filtered", argv[1]) == 0) {
            filtered = true;
        } else {
            filtered = false; // punt
        }
    } else {
        filtered = false;
    }

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

    qpsk->m = (int) (qpsk->fs / qpsk->rs); /* 5 */
    qpsk->inv_m = (1.0f / (float) qpsk->m);

    qpsk->ntxtbits = 4;
    qpsk->ftwindowwidth = 11;
    qpsk->timing_mx_thresh = 0.30f;

    /* create the QPSK pilot time-domain waveform */

    complex float symbol;

    FILE *fout = fopen("/tmp/spectrum.raw", "wb");

    flush_tx_filter();

    for (int k = 0; k < 500; k++) {
        sample_offset = 0;

        // 33 BPSK pilots
        for (int i = 0; i < 33; i++) {
            symbol = constellation[(pilotvalues[i] == 1) ? 0 : 3];

            tx_symbol(qpsk, symbol, filtered);
        }

        // 31 QPSK
        for (int i = 0; i < 31; i++) {
            symbol = constellation[rand() % 4];

            tx_symbol(qpsk, symbol, filtered);
        }

        fwrite(tx_samples, sizeof (int16_t), sample_offset, fout);
    }

    free(qpsk);
    fclose(fout);

    return (EXIT_SUCCESS);
}
