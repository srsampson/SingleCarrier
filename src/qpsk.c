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

#include "qpsk_internal.h"
#include "fir.h"
#include "costas.h"

// Prototypes

static float cnormf(complex float);
static float correlate_pilots(complex float [], int);
static float find_quadrant_and_distance(int *, complex float);
static void rx_frame(int16_t [], int []);

// Externals

extern const int8_t pilotvalues[];
extern const complex float constellation[];

// Globals

static FILE *fin;
static FILE *fout;

static State state;

static complex float tx_filter[NTAPS];
static complex float rx_filter[NTAPS];
static complex float input_frame[(FRAME_SIZE * 2)];
static complex float decimated_frame[562];

#if defined(COSTAS)
static complex float costas_frame[562];
static complex float freq_frame[562];
#endif

static complex float pilot_table[PILOT_SYMBOLS];
static complex float rx_pilot[PILOT_SYMBOLS];

// Two phase for full duplex

static complex float fbb_tx_phase;
static complex float fbb_tx_rect;

static complex float fbb_rx_phase;
static complex float fbb_rx_rect;
    
static int rx_timing;

/*
 * Select which FIR coefficients
 * true = alpha50_root
 * false = alpha35_root
 */
static bool alpha50 = false;

// Defines

/*
 * Experimental RX Frequency Offset from TX
 */
#define FOFFSET 0.0f

#ifdef DEBUG2
    int pilot_frames_detected = 0;
#endif

// Functions

static float cnormf(complex float val) {
    float realf = crealf(val);
    float imagf = cimagf(val);

    return realf * realf + imagf * imagf;
}

/*
 * Sliding Window
 *
 * Returns the mean magnitude of the
 * underlying pilot symbols in window
 */
static float correlate_pilots(complex float symbol[], int index) {
    complex float out = 0.0f;

    for (int i = 0, j = index; i < PILOT_SYMBOLS; i++, j++) {
        out += (pilot_table[i] * symbol[j]);
    }

    return cnormf(out);
}

/*
 * Receive function
 * 
 * Basically we receive a 1600 baud QPSK at 8000 samples/sec.
 * 
 * Each frame is made up of 33 Pilots and 31 x 8 Data symbols.
 * This is (33 * 5) = 165 + (31 * 5 * 8) = 1240 or 1405 samples per packet
 */
static void rx_frame(int16_t in[], int bits[]) {
    /*
     * Convert input PCM to complex samples
     * Translate to baseband at an 8 kHz sample rate
     */
    for (int i = 0; i < FRAME_SIZE; i++) {
        fbb_rx_phase *= fbb_rx_rect;
        
        complex float val = fbb_rx_phase * ((float) in[i] / 16384.0f);

        input_frame[i] = input_frame[FRAME_SIZE + i];
        input_frame[FRAME_SIZE + i] = val;
    }

    fbb_rx_phase /= cabsf(fbb_rx_phase); // normalize as magnitude can drift

    /*
     * Raised Root Cosine Filter
     */
    fir(rx_filter, alpha50, input_frame, FRAME_SIZE);

    /*
     * Decimate by 5 to the 1600 symbol rate
     */
    for (int i = 0; i < (FRAME_SIZE / CYCLES); i++) {
	int extended = (FRAME_SIZE / CYCLES) + i;  // compute once

        decimated_frame[i] = decimated_frame[extended];
        decimated_frame[extended] = input_frame[(i * CYCLES) + rx_timing];
    }

#ifdef COSTAS
    /*
     * Correct for phase and frequency offset
     */
    costas((FRAME_SIZE / CYCLES), decimated_frame, costas_frame, freq_frame);    

    float nco = get_freq();
    float phase = get_phase();

#ifdef DEBUG1
    printf("%.4f, %.4f,\n", nco, phase);
#endif

#ifdef TEST_SCATTER
    for (int i = 0; i < (FRAME_SIZE / CYCLES); i++) {
        fprintf(stderr, "%f %f\n", crealf(costas_frame[i]), cimagf(costas_frame[i]));
#endif
    }
#endif
    
#if !defined(COSTAS) && defined(TEST_SCATTER)
    for (int i = 0; i < (FRAME_SIZE / CYCLES); i++) {
        fprintf(stderr, "%f %f\n", crealf(decimated_frame[i]), cimagf(decimated_frame[i]));
    }
#endif
    
    /* Hunting for the pilot as a preamble sequence */

    float temp_value = 0.0f;
    float max_value = 0.0f;
    int max_index = 0;

    for (int i = 0; i < ((FRAME_SIZE / CYCLES) / 2); i++) {
#if defined(COSTAS)
        temp_value = correlate_pilots(costas_frame, i);
#else
        temp_value = correlate_pilots(decimated_frame, i);
#endif
        if (temp_value > max_value) {
            max_value = temp_value;
            max_index = i;
        }
    }

    if (max_value > 280.0f) {
        /*
         * Educate the decimator with the correlation
         */
        rx_timing = max_index;
    
#ifdef DEBUG2
        pilot_frames_detected++;
        printf("Frames: %d MaxIdx: %d MaxVal: %.2f Mean: %.2f\n",
            pilot_frames_detected, max_index, max_value, mean);
#endif

        for (int i = 0, j = max_index; j < (PILOT_SYMBOLS + max_index); i++, j++) {
            /*
             * Save the pilots for the coherent process
             */
#if defined(COSTAS)
            rx_pilot[i] = costas_frame[j];
#else
            rx_pilot[i] = decimated_frame[j];
#endif
        }
        
        /*
         * Now process data symbols TODO
         */
        
        state = process;
    } else {
        /*
         * Burn remainder of frame
         */
        state = hunt;
    }
 }

/*
 * Gray coded QPSK modulation function
 */
complex float qpsk_mod(int bits[]) {
    return constellation[(bits[1] << 1) | bits[0]];
}

/*
 * The smallest distance between constellation
 * and the symbol, is our gray coded quadrant.
 * 
 *      1
 *      |
 *  3---+---0
 *      |
 *      2
 *
 * Due to adding the I+Q at the transmitter, the constellation
 * Will actually be rotated left 45 degrees.
 */
 static float find_quadrant_and_distance(int *quadrant, complex float symbol) {
    float min_value = 200.0f; // some large value
    float distance;

    for (int i = 0; i < 4; i++) {
        distance = cnormf(symbol - constellation[i]);

        if (distance < min_value) {
            min_value = distance;
            *quadrant = i;
        }
    }

    return min_value;
}

/*
 * Modulate the symbols by first upsampling to 8 kHz sample rate,
 * and translating the spectrum to 1100 Hz, where it is filtered
 * using the root raised cosine coefficients.
 */
int tx_frame(int16_t samples[], complex float symbol[], int length) {
    complex float signal[(length * CYCLES)];

    /*
     * Build the 1600 baud packet Frame zero padding
     * for the desired 8 kHz sample rate.
     */
    for (int i = 0; i < length; i++) {
        signal[(i * CYCLES)] = symbol[i];

        for (int j = 1; j < CYCLES; j++) {
            signal[(i * CYCLES) + j] = 0.0f;
        }
    }

    /*
     * Root Cosine Filter
     * true = 50% rolloff, false = 35% rolloff
     */
    fir(tx_filter, alpha50, signal, (length * CYCLES));

    /*
     * Shift Baseband to Center Frequency
     */
    for (int i = 0; i < (length * CYCLES); i++) {
        fbb_tx_phase *= fbb_tx_rect;
        signal[i] *= fbb_tx_phase;
    }

    fbb_tx_phase /= cabsf(fbb_tx_phase); // normalize as magnitude can drift

    /*
     * Now return the resulting real samples
     */
    for (int i = 0; i < (length * CYCLES); i++) {
        samples[i] = (int16_t) ((crealf(signal[i]) + cimagf(signal[i])) * 16384.0f); // @ .5
    }

    return (length * CYCLES);
}

int bpsk_modulate(int16_t samples[]) {
    return tx_frame(samples, pilot_table, PILOT_SYMBOLS);
}

int qpsk_modulate(int16_t samples[], int tx_bits[], int length) {
    complex float symbol[length];
    int dibit[2];

    for (int i = 0, s = 0; i < length; i++, s += 2) {
        dibit[0] = tx_bits[s + 1] & 0x1;
        dibit[1] = tx_bits[s ] & 0x1;

        symbol[i] = qpsk_mod(dibit);
    }

    return tx_frame(samples, symbol, length);
}

// Main Program

int main(int argc, char** argv) {
    int bits[6400];
    int16_t frame[FRAME_SIZE];
    int length;

    srand(time(0));

#if defined(COSTAS)
    /*
     * Experiment with this BW number
     */
    //float bw = TAU / 100.0f;    // (2pi/100) to (2pi/200)

    /*
     * Stumped
     */
    //float damp = sqrtf(2.0f) / 2.0f;
    float alpha = .1f; //(4.0f * damp * bw) / (1.0f + 2.0f * damp * bw + bw * bw);
    float beta = .25f * (alpha * alpha); //(4.0f * bw * bw) / (1.0f + 2.0f * damp * bw + bw * bw);
    
    /*
     * Hell, I don't know?? TODO
     */
    float max_freq = +.25f;
    float min_freq = -.25f;

    costas_create(alpha, beta, max_freq, min_freq);
#endif
    
    /*
     * Create a complex table of pilot values
     * for the correlation algorithm
     */
    for (int i = 0; i < PILOT_SYMBOLS; i++) {
        pilot_table[i] = (float) pilotvalues[i]; // complex -1.0 or 1.0
    }

    /*
     * create the BPSK pilot and QPSK data waveform.
     * This simulates the transmitted packets.
     */
    fout = fopen(TX_FILENAME, "wb");

    fbb_tx_phase = cmplx(0.0f);
    fbb_tx_rect = cmplx(TAU * CENTER / FS);

    for (int k = 0; k < 500; k++) {
        // 33 BPSK pilots
        length = bpsk_modulate(frame);

        fwrite(frame, sizeof (int16_t), length, fout);

        /*
         * NS data frames between each pilot frame
         */
        for (int j = 0; j < NS; j++) {
            // 31 QPSK
            for (int i = 0; i < (DATA_SYMBOLS * 2); i += 2) {
                bits[i] = rand() % 2;
                bits[i + 1] = rand() % 2;
            }

            length = qpsk_modulate(frame, bits, DATA_SYMBOLS);

            fwrite(frame, sizeof (int16_t), length, fout);
        }
    }

    fclose(fout);

    /*
     * Now try to process what was transmitted
     */
    fin = fopen(TX_FILENAME, "rb");

    fbb_rx_phase = cmplx(0.0f);
    fbb_rx_rect = cmplx(TAU * (-CENTER + FOFFSET) / FS);

    while (1) {
        /*
         * Read in the frame samples
         */
        size_t count = fread(frame, sizeof (int16_t), FRAME_SIZE, fin);

        if (count != FRAME_SIZE)
            break;

        rx_frame(frame, bits);
    }

    fclose(fin);

    return (EXIT_SUCCESS);
}

