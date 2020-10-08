/*
 * qpsk.c
 *
 * Testing program for qpsk modem algorithms
 * October 2020
 */

// Includes

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <complex.h>
#include <string.h>
#include <time.h>

#include "qpsk.h"

// Prototypes

static float cnormf(complex float);
static complex float fir(complex float *, complex float [], int);
static float correlate_pilots(complex float [], int);
static complex float vector_sum(complex float *, int);

// Defines
#define TX_FILENAME "/tmp/spectrum-filtered.raw"
#define RX_FILENAME "/tmp/spectrum.raw"

// Globals

static FILE *fin;
static FILE *fout;

static complex float osc_table[OSC_TABLE_SIZE];
static complex float tx_filter[NZEROS];
static complex float rx_filter[NZEROS];
static complex float input_frame[RX_SAMPLES_SIZE * 2];
static complex float process_frame[RX_SAMPLES_SIZE];

static complex float pilot_table[PILOT_SYMBOLS];

static int16_t tx_samples[TX_SAMPLES_SIZE];

static int tx_osc_offset;
static int rx_osc_offset;
static int tx_sample_offset;

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

/*
 * Digital filter designed by mkfilter/mkshape/gencode
 * A.J. Fisher
 */
static const float rrccoeff[] = {
    0.0020423298f, 0.0119232360f, 0.0133470732f, 0.0030905368f,
    -0.0138171019f, -0.0254514870f, -0.0196589480f, 0.0071235033f,
    0.0442307140f, 0.0689148706f, 0.0571180020f, -0.0013440359f,
    -0.0909920934f, -0.1698743515f, -0.1827969854f, -0.0846912701f,
    0.1357337643f, 0.4435364036f, 0.7637556509f, 1.0056258619f,
    1.0956360553f, 1.0056258619f, 0.7637556509f, 0.4435364036f,
    0.1357337643f, -0.0846912701f, -0.1827969854f, -0.1698743515f,
    -0.0909920934f, -0.0013440359f, 0.0571180020f, 0.0689148706f,
    0.0442307140f, 0.0071235033f, -0.0196589480f, -0.0254514870f,
    -0.0138171019f, 0.0030905368f, 0.0133470732f, 0.0119232360f,
    0.0020423298f
};

// Functions

static float cnormf(complex float val) {
    float realf = crealf(val);
    float imagf = cimagf(val);

    return realf * realf + imagf * imagf;
}

/*
 * Root Raised Cosine FIR
 */
static complex float fir(complex float *memory, complex float sample[], int index) {
    for (int i = 0; i < (NZEROS - 1); i++) {
        memory[i] = memory[i + 1];
    }

    memory[(NZEROS - 1)] = sample[index];

    complex float y = 0.0f;

    for (int i = 0; i < NZEROS; i++) {
        y += (memory[i] * rrccoeff[i]);
    }

    return y;
}

/*
 * Sliding Window
 */
static float correlate_pilots(complex float symbol[], int index) {
    complex float out = 0.0f;

    for (int i = 0, j = index; i < PILOT_SYMBOLS; i++, j++) {
        out += (symbol[j] * pilot_table[i]);
    }

    return cnormf(out);
}

/*
 * Receive function
 */
void rx_frame(int16_t in[], int *bits, FILE *fout) {
    int16_t pcm[(RX_SAMPLES_SIZE / CYCLES)];

    for (int i = 0; i < RX_SAMPLES_SIZE; i++) {
        float val = (float) in[i] / SCALE;

        input_frame[i] = input_frame[RX_SAMPLES_SIZE + i];
        input_frame[RX_SAMPLES_SIZE + i] = osc_table[rx_osc_offset] * val;
        rx_osc_offset = (rx_osc_offset + 1) % OSC_TABLE_SIZE;
    }

    // Downsample the 5 cycles at 8 kHz Sample rate for 1600 Baud

    for (int i = 0; i < (RX_SAMPLES_SIZE / CYCLES); i++) {
        process_frame[i] = process_frame[(RX_SAMPLES_SIZE / CYCLES) + i];
        process_frame[(RX_SAMPLES_SIZE / CYCLES) + i] = fir(rx_filter, input_frame, (i * CYCLES));

        // testing
        pcm[i] = (int16_t) (crealf(process_frame[i]) * 1024.0f);
    }
    
    fwrite(pcm, sizeof (int16_t), (RX_SAMPLES_SIZE / CYCLES), fout);

    /* Hunting for the pilot preamble sequence */

    float temp_value = 0.0f;
    float max_value = 0.0f;
    int max_index = 0;

    for (int i = 0; i < PILOT_SYMBOLS; i++) {
        temp_value = correlate_pilots(process_frame, i);

        if (temp_value > max_value) {
            max_value = temp_value;
            max_index = i;
        }
    }

    // find BPSK pilots

    printf("Max Index = %d\n", max_index);
    
    /*
     * figure out QPSK bits
     */
    //for (int i = 0; i < (DATA_SYMBOLS * NS); i++) {
        
    //}
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

void tx_frame(complex float symbol[], int length) {
    for (int k = 0; k < length; k++) {
        /*
         * At the 8 kHz sample rate, we will need to
         * upsample 5 cycles of the symbol for 1600 baud
         */
        for (int j = 0; j < CYCLES; j++) {
            complex float y = fir(tx_filter, symbol, k) * osc_table[tx_osc_offset];
            tx_osc_offset = (tx_osc_offset + 1) % OSC_TABLE_SIZE;

            tx_samples[tx_sample_offset] = (int16_t) (crealf(y) * SCALE);
            tx_sample_offset = (tx_sample_offset + 1) % TX_SAMPLES_SIZE;
        }
    }
}

/*
 * Zero out the selected FIR delay memory
 */
void flush_fir_memory(complex float *memory) {
    for (int i = 0; i < NZEROS; i++) {
        memory[i] = 0.0f;
    }
}

void tx_frame_reset() {
    tx_sample_offset = 0;
}

/*
 * Transmit null
 */
void tx_flush() {
    complex float symbol[CYCLES];

    for (int i = 0; i < CYCLES; i++) {
        symbol[i] = 0.0f;
    }

    tx_frame(symbol, CYCLES);
}

void bpsk_modulate(int tx_bits[], int length) {
    complex float symbol[length];

    for (int i = 0; i < length; i++) {
        symbol[i] = (float) tx_bits[i];
    }

    tx_frame(symbol, length);
}

void qpsk_modulate(int tx_bits[], int length) {
    complex float symbol[length];
    int dibit[2];

    for (int i = 0, s = 0; i < length; i++, s += 2) {
        dibit[0] = tx_bits[s + 1] & 0x1;
        dibit[1] = tx_bits[s ] & 0x1;

        symbol[i] = qpsk_mod(dibit);
    }

    tx_frame(symbol, length);
}

// Main Program

int main(int argc, char** argv) {
    int bits[6400];
    int16_t frame[RX_SAMPLES_SIZE];

    srand(time(0));

    tx_osc_offset = 0;
    rx_osc_offset = 0;

    /*
     * Create an oscillator table for the selected
     * center frequency of 1200 Hz
     */
    for (int i = 0; i < OSC_TABLE_SIZE; i++) {
        osc_table[i] = cmplx(TAU * CENTER * ((float) i / FS));
    }

    /*
     * Create a float table of pilot values
     */
    for (int i = 0; i < PILOT_SYMBOLS; i++) {
        pilot_table[i] = (float) pilotvalues[i]; // complex -1 or 1
    }

    /*
     * create the BPSK/QPSK pilot time-domain waveform
     */

    fout = fopen(TX_FILENAME, "wb");

    flush_fir_memory(tx_filter);
    tx_flush();

    for (int k = 0; k < 500; k++) {
        tx_frame_reset();

        // 33 BPSK pilots
        for (int i = 0; i < PILOT_SYMBOLS; i++) {
            bits[i] = pilotvalues[i];
        }

        bpsk_modulate(bits, PILOT_SYMBOLS);
        tx_flush();

        /*
         * NS data frames between each pilot frame
         */
        for (int j = 0; j < NS; j++) {
            // 31 QPSK
            for (int i = 0; i < (DATA_SYMBOLS * 2); i += 2) {
                bits[i] = rand() % 2;
                bits[i + 1] = rand() % 2;
            }

            qpsk_modulate(bits, DATA_SYMBOLS);
        }

        fwrite(tx_samples, sizeof (int16_t), tx_sample_offset, fout);
    }

    fflush(fout);
    fclose(fout);

    /*
     * Now try to process what was transmitted
     */
    fin = fopen(TX_FILENAME, "r");
    fout = fopen(RX_FILENAME, "wb");

    while (1) {
        size_t count = fread(frame, sizeof (int16_t), RX_SAMPLES_SIZE, fin);

        if (count != RX_SAMPLES_SIZE)
            break;

        rx_frame(frame, bits, fout);
    }

    fflush(fout);
    fclose(fout);
    fclose(fin);

    return (EXIT_SUCCESS);
}
