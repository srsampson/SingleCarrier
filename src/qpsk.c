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
static complex float dft(complex float *);
static void idft(complex float *, complex float *, int);
static float correlate_pilots(complex float [], int);
static float magnitude_pilots(complex float [], int);

// Defines
#define TX_FILENAME "/tmp/spectrum.raw"

// Globals

static FILE *fin;
static FILE *fout;

static complex float tx_filter[NZEROS];
static complex float rx_filter[NZEROS];
static complex float input_frame[RX_SAMPLES_SIZE * 2];  // Input Samples * 2
static complex float process_frame[RX_SAMPLES_SIZE];    // Input Symbols * 2
static complex float pilot_table[PILOT_SYMBOLS];
static int16_t tx_samples[TX_SAMPLES_SIZE];
static int tx_sample_offset;
static float inv_cycles;

/*
 * QPSK Quadrant bit-pair values - Gray Coded
 * Non-static so they can be used by other modules
 */
const complex float constellation[] = {
    1.0f + 0.0f * I,    //  I
    0.0f + 1.0f * I,    //  Q
    0.0f - 1.0f * I,    // -Q
    -1.0f + 0.0f * I    // -I
};

/*
 * These pilots are compatible with Octave version
 * Non-static so they can be used by other modules
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
 * 
 * Root Raised Cosine Coefficients
 * 
 * 8 kHz sample rate, 1600 Baud with impulse length of 41,
 * using .35 beta
 * 
 * A.J. Fisher Design
 */
static const float rrccoeff[] = {
+0.0232940453, -0.0201202258, -0.0612790348, -0.0819379363,
    -0.0698817641, -0.0250023072, +0.0387785530, +0.0970884688,
    +0.1235018201, +0.1000860094, +0.0262756547, -0.0775463161,
    -0.1737859595, -0.2182154794, -0.1743657855, -0.0270615201,
    +0.2096612579, +0.4934135321, +0.7634412783, +0.9570270562,
    +1.0273271784, +0.9570270562, +0.7634412783, +0.4934135321,
    +0.2096612579, -0.0270615201, -0.1743657855, -0.2182154794,
    -0.1737859595, -0.0775463161, +0.0262756547, +0.1000860094,
    +0.1235018201, +0.0970884688, +0.0387785530, -0.0250023072,
    -0.0698817641, -0.0819379363, -0.0612790348, -0.0201202258,
    +0.0232940453
};

// Functions

static float cnormf(complex float val) {
    float realf = crealf(val);
    float imagf = cimagf(val);

    return realf * realf + imagf * imagf;
}

/*
 * FIR Filter with specified impulse length
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
 * Convert frequency sample into cycles of time domain result
 */
static void idft(complex float *result, complex float symbol[], int index) {
    result[0] = (symbol[index] * inv_cycles);

    for (int row = 1; row < CYCLES; row++) {
        result[row] = cmplx(DOC * row) * (symbol[index] * inv_cycles);
    }
}

/*
 * Convert time samples into frequency domain result
 */
static complex float dft(complex float *samples) {
    complex float result = samples[0];

    complex float c = cmplxconj(DOC);
    complex float delta = c;

    for (int row = 1; row < CYCLES; row++) {
        result += (samples[row] * c);
        c *= delta;
    }
    
    return result;
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

static float magnitude_pilots(complex float symbol[], int index) {
    complex float out = 0.0f;
    
    for (int i = index; i < (PILOT_SYMBOLS + index); i++) {
        out += symbol[i];
    }

    return cnormf(out);
}

/*
 * Receive function
 */
void rx_frame(int16_t in[], int bits[]) {
    complex float sample[1];

    /*
     * Filter at the 8 kHz rate
     */
    for (int i = 0; i < RX_SAMPLES_SIZE; i++) {
        sample[0] = ((float) in[i] / 16384.0f) + 0.0f * I;
        
        input_frame[i] = input_frame[RX_SAMPLES_SIZE + i];
        input_frame[RX_SAMPLES_SIZE + i] = sample[0];//fir(rx_filter, sample, 0);
    }

    /*
     * Downsample
     */
    for (int i = 0, j = 0; i < RX_SAMPLES_SIZE; i += CYCLES, j++) {
        process_frame[j] = process_frame[(RX_SAMPLES_SIZE / CYCLES) + j];
        process_frame[(RX_SAMPLES_SIZE / CYCLES) + j] = dft(&input_frame[i]);
    }
    
    /*
     * We are now dealing with 1600 symbol rate after DFT
     */

    int dibit[2];
    
    /* Hunting for the pilot preamble sequence */

    float temp_value = 0.0f;
    float max_value = 0.0f;
    float mean = 0.0f;
    int max_index = 0;

    for (int i = 0; i < (RX_SAMPLES_SIZE / CYCLES); i++) {
        temp_value = correlate_pilots(process_frame, i);

        if (temp_value > max_value) {
            max_value = temp_value;
            max_index = i;
        }
    }

    mean = magnitude_pilots(process_frame, max_index);

    //if (mean > 30.0f) {
        printf("Max_index = %d Mean = %.2f\n", max_index, mean);

        for (int i = max_index; i < (max_index + PILOT_SYMBOLS); i++) {
            complex float symbol = process_frame[i];
            qpsk_demod(symbol, dibit);

            printf("%d%d ", dibit[1], dibit[0]);
        }

        printf("\n\n");
        
        fflush(stdout);
    //}
}

/*
 * Gray coded QPSK modulation function
 * 
 *      Q
 *      |
 * -I---+---I
 *      |
 *     -Q
 * 
 * The symbols are not rotated on transmit
 */
complex float qpsk_mod(int bits[]) {
    return constellation[(bits[1] << 1) | bits[0]];
}

/*
 * Gray coded QPSK demodulation function
 *
 *  Q 01   00 I
 *     \ | /
 *      \|/
 *   ----+----
 *      /|\
 *     / | \
 * -I 11   10 -Q
 * 
 * By rotating received symbol 45 degrees left the
 * bits are easier to decode as they are in a specific
 * rectangular quadrant.
 * 
 * Each bit pair differs from the next by only one bit.
 */
void qpsk_demod(complex float symbol, int bits[]) {
    complex float rotate = symbol * cmplx(ROTATE45);

    bits[0] = crealf(rotate) < 0.0f;    // I < 0 ?
    bits[1] = cimagf(rotate) < 0.0f;    // Q < 0 ?
}

/*
 * The symbol rate is 1600 baud
 */
void tx_frame(complex float symbol[], int length) {
    complex float result[CYCLES];
    complex float signal[CYCLES * length];

    /*
     * Here we go from the 1600 symbol rate by
     * upsampling each symbol to the 8 kHz sample rate
     */

    for (int i = 0, k = 0; i < length; i++, k += CYCLES) {
        idft(result, symbol, i);

        for (int j = 0; j < CYCLES; j++) {
            signal[k+j] = result[j];
        }
    }

    /*
     * Filter at the 8 kHz rate
     */
    for (int i = 0; i < (CYCLES * length); i++) {
        complex float sym = signal[i];//fir(tx_filter, signal, i);
        
        tx_samples[tx_sample_offset] = (int16_t) (crealf(sym) * 32767.0f);
        tx_sample_offset = (tx_sample_offset + 1) % TX_SAMPLES_SIZE;
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
 * Transmit CYCLES (5) null symbols
 */
void tx_flush() {
    complex float symbol[CYCLES];

    for (int i = 0; i < CYCLES; i++) {
        symbol[i] = 0.0f;
    }

    tx_frame(symbol, CYCLES);
}

void bpsk_pilot_modulate() {
    tx_frame(pilot_table, PILOT_SYMBOLS);
}

void qpsk_data_modulate(int tx_bits[], int length) {
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

    /*
     * Create a complex float table of pilot values
     * for correlation algorithm
     */
    for (int i = 0; i < PILOT_SYMBOLS; i++) {
        pilot_table[i] = ((float) pilotvalues[i] + 0.0f * I); // complex -1.0 or 1.0
    }

    inv_cycles = (1.0f / CYCLES);
    
    /*
     * create the BPSK/QPSK pilot time-domain waveform
     */
    fout = fopen(TX_FILENAME, "wb");

    /*
     * Initialize the tx FIR filter memory for
     * this transmission of packets
     */
    flush_fir_memory(tx_filter);
    
    /*
     * This simulates the transmitted packets
     */
    
    for (int k = 0; k < 500; k++) {
        tx_frame_reset();
        tx_flush();
    
        // 33 BPSK pilots
        bpsk_pilot_modulate();
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
            
            qpsk_data_modulate(bits, DATA_SYMBOLS);
        }

        fwrite(tx_samples, sizeof (int16_t), tx_sample_offset, fout);
    }

    fflush(fout);
    fclose(fout);

    /*
     * Now try to process what was transmitted
     */
    fin = fopen(TX_FILENAME, "rb");
    
    /*
     * Initialize the tx FIR filter memory for
     * this transmission of packets
     */
    flush_fir_memory(rx_filter);

    while (1) {
        size_t count = fread(frame, sizeof (int16_t), RX_SAMPLES_SIZE, fin);

        if (count != RX_SAMPLES_SIZE)
            break;

        rx_frame(frame, bits);
    }

    fclose(fin);

    return (EXIT_SUCCESS);
}
