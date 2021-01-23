//#define DEBUG
//#define DEBUG2
//#define DEBUG3
//#define TEST_SCATTER
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
#include "fft.h"

// Prototypes

static float cnormf(complex float);
static float correlate_pilots(complex float [], int);
static float magnitude_pilots(complex float [], int);
static void frequency_shift(complex float [], complex float [], int);

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
static complex float decimated_frame[576]; // (FRAME_SIZE / CYCLES) * 2
static complex float pilot_table[PILOT_SYMBOLS];
static complex float rx_pilot[PILOT_SYMBOLS];

// Two phase for full duplex

static complex float fbb_tx_phase;
static complex float fbb_tx_rect;

static complex float fbb_rx_phase;
static complex float fbb_rx_rect;

static float freqEstimate;

#ifdef DEBUG
    int pilot_frames_detected = 0;
#endif

fft_cfg fft_forward;
fft_cfg fft_inverse;

// Functions

static float cnormf(complex float val) {
    float realf = crealf(val);
    float imagf = cimagf(val);

    return realf * realf + imagf * imagf;
}

/*
 * Sliding Window
 */
static float correlate_pilots(complex float symbol[], int index) {
    complex float out = 0.0f;

    for (int i = 0, j = index; i < PILOT_SYMBOLS; i++, j++) {
        out += (pilot_table[i] * symbol[j]);
    }

    return cnormf(out);
}

/*
 * Save time, skip sqrt()
 */
static float magnitude_pilots(complex float symbol[], int index) {
    float out = 0.0f;

    for (int i = index; i < (PILOT_SYMBOLS + index); i++) {
        out += cnormf(symbol[i]);
    }
    
    return out;
}

/*
 * Shift the center frequency to new estimate
 */
static void frequency_shift(complex float waveform[], complex float signal[], int lnin) {
    complex float phase = cmplxconj(TAU * freqEstimate / FS);

    for (int i = 0; i < lnin; i++) {
        fbb_rx_phase *= phase;
        waveform[i] = signal[i] * fbb_rx_phase;
    }

    fbb_rx_phase /= cabsf(fbb_rx_phase);
}

/*
 * Receive function
 * 
 * Basically we receive a 1600 baud QPSK at 8000 samples/sec.
 * 
 * Each frame is made up of 32 Pilots and 32 x 8 Data symbols.
 * This is (32 * 5) = 160 + (32 * 5 * 8) = 1280 or 1440 samples per packet
 */
void rx_frame(int16_t in[], int bits[]) {
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
     * Raised Root Cosine Filter at 8 khz
     */
    fir(rx_filter, input_frame, FRAME_SIZE);

    /*
     * Decimate by 5 to the 1600 symbol rate
     */
    for (int i = 0; i < (FRAME_SIZE / CYCLES); i++) {   // 288 samples
	int extended = (FRAME_SIZE / CYCLES) + i;  // compute once

        decimated_frame[i] = decimated_frame[extended];
        decimated_frame[extended] = input_frame[(i * CYCLES) + FINE_TIMING_OFFSET];

#ifdef TEST_SCATTER
        fprintf(stderr, "%f %f\n", crealf(decimated_frame[extended]), cimagf(decimated_frame[extended]));
#endif 
    }

    complex float decframe[512] = {0.0f};

    float max = 0.0f;
    int index = 0;

    /*
     * zero pad 288 -> 511
     */
    for (int i = 0; i < (FRAME_SIZE / CYCLES); i++) {
        decframe[i] = decimated_frame[i];
    }

    /*
     * 1600 / 512 = 3.125 Hz per filter
     */
    fft(fft_forward, decframe, decframe);

    /*
     * Move DC to center
     */
    for (int i = 0; i < 256; i++) {
        complex float temp = decframe[i];
        
        decframe[i] = decframe[i + 256];
        decframe[i + 256] = temp;
    }
    
#ifdef DEBUG3
    for (int i = 0; i < 512; i++) {
        fprintf(stdout, "%d, %f\n", i, cabs(decframe[i]));
    }
#endif
    
    /*
     * Find the max magnitude (frequency)
     * and complex frequency index
     */
    for (int i = 0; i < 512; i++) {
        float temp = cabs(decframe[i]);
        
        if (temp > max) {
            max = temp;
            index = i;
        }
    }
    
#ifdef DEBUG2
    if (index >= 256) {
        fprintf(stdout, "+%.1f\n", (256 - index) * 3.125f);
    } else {
        fprintf(stdout, "-%.1f\n", index * 3.125f);
    }
#endif

    /* Hunting for the pilot preamble sequence */

    float temp_value = 0.0f;
    float max_value = 0.0f;
    float mean = 0.0f;
    int max_index = 0;

    for (int i = 0; i < ((FRAME_SIZE / CYCLES) / 2); i++) {
        temp_value = correlate_pilots(decimated_frame, i);

        if (temp_value > max_value) {
            max_value = temp_value;
            max_index = i;
        }
    }

    mean = magnitude_pilots(decimated_frame, max_index);

#ifdef DEBUG
    pilot_frames_detected++;
#endif

    if (max_value > (mean * 10.0f)) {
#ifdef DEBUG
        printf("Frames: %d MaxIdx: %d MaxVal: %.2f Mean: %.2f\n",
                pilot_frames_detected, max_index, max_value, mean);
#endif
        for (int i = 0, j = max_index; j < (PILOT_SYMBOLS + max_index); i++, j++) {
            /*
             * Save the pilots for the coherent process
             */
            rx_pilot[i] = decimated_frame[j];
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
    
    fflush(stdout);
    fflush(stderr);
}

/*
 * Gray coded QPSK modulation function
 * 
 *      Q
 *      |
 * -I---+---I
 *      |
 *     -Q
 */
complex float qpsk_mod(int bits[]) {
    return constellation[(bits[1] << 1) | bits[0]];
}

/*
 * Gray coded QPSK demodulation function
 */
void qpsk_demod(complex float symbol, int bits[]) {
    complex float rotate = symbol * cmplx(ROT45);
    
    bits[0] = crealf(rotate) < 0.0f; // I < 0 ?
    bits[1] = cimagf(rotate) < 0.0f; // Q < 0 ?
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
     */
    fir(tx_filter, signal, (length * CYCLES));

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
        samples[i] = (int16_t) (crealf(signal[i]) * 16384.0f); // @ .5
    }

    return (length * CYCLES);
}

int bpsk_pilot_modulate(int16_t samples[]) {
    return tx_frame(samples, pilot_table, PILOT_SYMBOLS);
}

int qpsk_data_modulate(int16_t samples[], int tx_bits[], int length) {
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

    fft_forward = fft_alloc(512, 0, NULL, NULL);
    fft_inverse = fft_alloc(512, 1, NULL, NULL);

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

    for (int k = 0; k < 50; k++) {
        // 33 BPSK pilots
        length = bpsk_pilot_modulate(frame);

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

            length = qpsk_data_modulate(frame, bits, DATA_SYMBOLS);

            fwrite(frame, sizeof (int16_t), length, fout);
        }
    }

    fclose(fout);

    /*
     * Now try to process what was transmitted
     */
    fin = fopen(TX_FILENAME, "rb");

    fbb_rx_phase = cmplx(0.0f);
    //fbb_rx_rect = cmplxconj(TAU * (CENTER + 2.0f) / FS);
    fbb_rx_rect = cmplxconj(TAU * CENTER / FS);

    while (1) {
        /*
         * Read in the frame samples (pilot + (data * ns)) * cycles
         */
        size_t count = fread(frame, sizeof (int16_t), FRAME_SIZE, fin);

        if (count != FRAME_SIZE)
            break;

        rx_frame(frame, bits);
    }

    fclose(fin);
    free(fft_forward);
    free(fft_inverse);

    return (EXIT_SUCCESS);
}
