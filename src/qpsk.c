#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <complex.h>
#include <string.h>
#include <time.h>

#include "qpsk.h"

static FILE *fout;

static complex float osc_table[OSC_TABLE_SIZE];
static complex float tx_filter[RRCLEN];

static int16_t tx_samples[TX_SAMPLES_SIZE];

static int tx_osc_offset;
static int rx_osc_offset;
static int tx_sample_offset;

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

static void tx_frame(complex float symbol[], int length) {
    complex float y;

    for (int k = 0; k < length; k++) {
        /*
         * At the 8 kHz sample rate, we will need
         * 5 cycles of the symbol for 1600 baud
         */
        for (int j = 0; j < CYCLES; j++) {
            for (int i = 0; i < (RRCLEN - 1); i++) {
                tx_filter[i] = tx_filter[i + 1];
            }

            tx_filter[(RRCLEN - 1)] = symbol[k];

            y = 0.0f;

            for (int i = 0; i < RRCLEN; i++) {
                y += tx_filter[i] * rrccoeff[i];
            }

            y *= osc_table[tx_osc_offset];
            tx_osc_offset = (tx_osc_offset + 1) % OSC_TABLE_SIZE;

            /*
             * 8192 will give 25% amplitude
             */
            tx_samples[tx_sample_offset] = (int16_t) (crealf(y) * SCALE);
            tx_sample_offset = (tx_sample_offset + 1) % TX_SAMPLES_SIZE;
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

/*
 * Transmit null
 */
static void flush() {
    complex float symbol[4];
    
    for (int i = 0; i < 4; i++) {
        symbol[i] = 0.0f;
    }

    tx_frame(symbol, 4);
}

void bpsk_modulate(int tx_bits[], int symbols) {
    complex float symbol[symbols];

    for (int i = 0; i < symbols; i++) {
        symbol[i] = (float) tx_bits[i];
    }

    tx_frame(symbol, symbols);
}

void qpsk_modulate(int tx_bits[], int symbols) {
    complex float symbol[symbols];
    int dibit[2];

    for (int s = 0, i = 0; i < symbols; s += 2, i++) {
        dibit[0] = tx_bits[s + 1] & 0x1;
        dibit[1] = tx_bits[s ] & 0x1;

        symbol[i] = qpsk_mod(dibit);
    }

    tx_frame(symbol, symbols);
}

int main(int argc, char** argv) {
    int bits[64];
    srand(time(0));

    tx_osc_offset = 0;
    rx_osc_offset = 0;

    for (int i = 0; i < OSC_TABLE_SIZE; i++) {
        osc_table[i] = cmplx(TAU * CENTER * ((float) i / FS));
    }

    /* create the QPSK pilot time-domain waveform */

    fout = fopen("/tmp/spectrum-filtered.raw", "wb");

    flush_tx_filter();
    flush();
    
    /*
     * This test transmits 64 symbols 500 times
     * 
     * Each symbol takes 1/1600 or 0.000625 sec
     * 0.000625 * 64 = .040 sec
     * .040 * 500 = 20 sec
     *
     * So you should see 20 seconds of audio recorded
     */
    for (int k = 0; k < 500; k++) {
        tx_sample_offset = 0;

        // 33 BPSK pilots
        for (int i = 0; i < 33; i++) {
            bits[i] = pilotvalues[i];
        }

        bpsk_modulate(bits, 33);
        flush();
        
        // 31 QPSK
        for (int i = 0; i < 62; i += 2) {
            bits[i] = rand() % 2;
            bits[i+1] = rand() % 2;
        }
        
        qpsk_modulate(bits, 31);
        flush();
        
        fwrite(tx_samples, sizeof (int16_t), tx_sample_offset, fout);
    }

    fclose(fout);

    return (EXIT_SUCCESS);
}
