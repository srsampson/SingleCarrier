/*
 * qpsk.c
 *
 * Licensed under GNU LGPL V2.1
 * See LICENSE file for information
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
#include "scramble.h"
#include "fir.h"

// Prototypes

static float correlate(complex float [], int);
static float magnitude(complex float [], int);
static int equalize(complex float [], int);

// Externals

extern const int8_t pilotvalues[];
extern const int8_t preamblevalues[];

// Locals

static FILE *fin;
static FILE *fout;

static RXState state;

static complex float tx_filter[NTAPS];
static complex float rx_filter[NTAPS];
static complex float input_frame[(FRAME_SIZE * 2)];
static complex float decimated_frame[562];
static complex float preambletable[PREAMBLE_LENGTH];

// Two phase for full duplex

static complex float fbb_tx_phase;
static complex float fbb_tx_rect;

static complex float fbb_rx_phase;
static complex float fbb_rx_rect;

static int rx_timing = FINE_TIMING_OFFSET;

/*
 * Select which FIR coefficients
 * true = (wide) alpha50_root
 * false = (narrow) alpha35_root
 */
static bool firwide = false;

// Defines

/*
 * Experimental RX Frequency Offset from TX
 */
#define FOFFSET 0.0f

#ifdef DEBUG2
int pilot_frames_detected = 0;
#endif

// Functions

float cnormf(complex float val) {
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
static float correlate(complex float symbol[], int index) {
    complex float out = 0.0f;

    for (size_t i = 0, j = index; i < PREAMBLE_LENGTH; i++, j++) {
        out += (preambletable[i] * symbol[j]);
    }

    return fabsf(cnormf(out));
}

/*
 * Return magnitude of symbols (sans sqrt)
 */
static float magnitude(complex float symbol[], int index) {
    float out = 0.0f;

    for (size_t i = index; i < (PREAMBLE_LENGTH + index); i++) {
        out += cnormf(symbol[i]);
    }

    return out;
}

static int equalize(complex float symbol[], int index) {
    int matches = 0;

    for (size_t i = 0, j = index; i < PREAMBLE_LENGTH; i++, j++) {
        complex float ref = preambletable[i];

        if ((equalize_preamble(symbol, j, ref) * crealf(ref)) > 0.0f) {
            matches++;
        }
    }

    return matches;
}

/*
 * Receive function
 *
 * Basically we receive a 1600 baud QPSK at 8000 samples/sec.
 *
 * Each frame is made up of 33 Pilots and 31 x 8 Data symbols.
 * This is (33 * 5) = 165 + (31 * 5 * 8) = 1240 or 1405 samples per packet
 */
int qpsk_rx_frame(int16_t in[], uint8_t bits[]) {
    /*
     * Convert input PCM to complex samples
     * Translate to baseband at an 8 kHz sample rate
     */
    for (size_t i = 0; i < FRAME_SIZE; i++) {
        fbb_rx_phase *= fbb_rx_rect;

        complex float val = fbb_rx_phase * ((float) in[i] / 16384.0f);

        input_frame[i] = input_frame[FRAME_SIZE + i];
        input_frame[FRAME_SIZE + i] = val;
    }

    fbb_rx_phase /= cabsf(fbb_rx_phase); // normalize as magnitude can drift

    /*
     * Raised Root Cosine Filter
     */
    fir(rx_filter, firwide, input_frame, FRAME_SIZE);

    /*
     * Decimate by 5 to the 1600 symbol rate
     */
    for (size_t i = 0; i < (FRAME_SIZE / CYCLES); i++) {
        int extended = (FRAME_SIZE / CYCLES) + i; // compute once

        decimated_frame[i] = decimated_frame[extended];
        decimated_frame[extended] = input_frame[(i * CYCLES) + rx_timing];
    }
    
#ifdef TEST_SCATTER
    for (int i = 0; i < (FRAME_SIZE / CYCLES); i++) {
        fprintf(stderr, "%f %f\n", crealf(decimated_frame[i]), cimagf(decimated_frame[i]));
    }
#endif

    /* Hunting for the preamble sequence */

    float temp_value = 0.0f;
    float max_value = 0.0f;
    int max_index = 0;

    for (size_t i = 0; i < PREAMBLE_LENGTH; i++) {
        temp_value = correlate(decimated_frame, i);

        if (temp_value > max_value) {
            max_value = temp_value;
            max_index = i;
        }
    }

    // data mode equalizer reset before burst
    kalman_reset();

    int matches = equalize(decimated_frame, max_index);

    float mean = magnitude(decimated_frame, max_index);

#ifdef DEBUG2
    pilot_frames_detected++;
#endif

    if ((matches > PREAMBLE_LENGTH - 30) && (max_value > mean * 20.0f)) {
#ifdef DEBUG2
        printf("Frames: %d Matches: %d MaxIdx: %d MaxVal: %.2f\n",
                pilot_frames_detected, matches, max_index, max_value);
#endif
        /*
         * Now process data symbols 
         */
        int sync_pos = max_index + PREAMBLE_LENGTH;

        for (size_t i = 0, k = sync_pos, bindex = 0; i < DATA_SYMBOLS; i++, k++, bindex += 2) {
            uint8_t dibit;

            data_eq(&dibit, decimated_frame, k);

            // Bits are encoded [IQ,IQ,...,IQ]

            bits[bindex + 1] = dibit >> 1;      // I Odd
            bits[bindex] = dibit & 0x1;         // Q Even
        }

        state = process;
        
        rx_timing = sync_pos;           // TODO 

        return 1;   // Valid frame
    } else {
        mean = 0.0f;
                
        for (size_t i = 0, k = rx_timing; i < DATA_SYMBOLS; i++, k++) {
            uint8_t dibit;

            mean += data_eq(&dibit, decimated_frame, k);
        }
        
        /* Check if reached the end of the frame */

        if (mean > EOF_COST_VALUE) {
            state = hunt;
        }
    }

    return 0;   // defaults to invalid frame
}

/*
 * Gray coded QPSK modulation function
 *
 * Bits are encoded [IQ,IQ,...,IQ]
 *
 * -1+j1  10 | 00  +1+j1
 *        ---+---
 * -1-j1  11 | 01  +1-j1
 *
 */
complex float qpsk_mod(uint8_t bits[], int index) {
    float i = (bits[index + 1] == 1) ? -1.0f : 1.0f;    //  I - Odd bits
    float q = (bits[index] == 1) ? -1.0f : 1.0f;        //  Q - Even bits

    return i + q * I;
}

/*
 * Gray coded QPSK demodulation function
 *
 * Bits are encoded [IQ,IQ,...,IQ]
 *
 * -1+j1  10 | 00  +1+j1
 *        ---+---
 * -1-j1  11 | 01  +1-j1
 *
 */
void qpsk_demod(uint8_t bits[], complex float symbol) {
    bits[1] = crealf(symbol) < 0.0f;    //  I - Odd bits
    bits[0] = cimagf(symbol) < 0.0f;    //  Q - Even bits
}

// Note that the mathematical approximation sin^−1(x) ≈ x, for small x
void qpsk_demod2(uint8_t bits[], complex float symbol) {
    float ans;
    
    complex float y = autocorrelation();
    
    if (crealf(y) >= 0.0f)
        ans = cimagf(symbol);
    else {
        ans = M_PI - cimagf(y);
    }
}

/*
 * Modulate the symbols by first upsampling to 8 kHz sample rate,
 * and translating the spectrum to 1100 Hz, where it is filtered
 * using the root raised cosine coefficients.
 */
int qpsk_tx_frame(int16_t samples[], complex float symbol[], int length) {
    complex float signal[(length * CYCLES)];

    /*
     * Build the 1600 baud packet Frame zero padding
     * for the desired 8 kHz sample rate.
     */
    for (size_t i = 0; i < length; i++) {
        signal[(i * CYCLES)] = symbol[i];

        for (size_t j = 1; j < CYCLES; j++) {
            signal[(i * CYCLES) + j] = 0.0f;
        }
    }

    /*
     * Raised Root Cosine Filter
     */
    fir(tx_filter, firwide, signal, (length * CYCLES));

    /*
     * Shift Baseband to Center Frequency
     */
    for (size_t i = 0; i < (length * CYCLES); i++) {
        fbb_tx_phase *= fbb_tx_rect;
        signal[i] *= fbb_tx_phase;
    }

    fbb_tx_phase /= cabsf(fbb_tx_phase); // normalize as magnitude can drift

    /*
     * Now return the resulting real samples
     */
    for (size_t i = 0; i < (length * CYCLES); i++) {
        samples[i] = (int16_t) (crealf(signal[i]) * 16384.0f); // @ .5
    }

    return (length * CYCLES);
}

/*
 * 128 Symbol Preamble
 */
int bpsk_modulate(int16_t samples[]) {
    return qpsk_tx_frame(samples, preambletable, PREAMBLE_LENGTH);
}

/*
 * Bits are IQ,IQ,...,IQ
 */
int qpsk_modulate(int16_t samples[], uint8_t tx_bits[], int length) {
    complex float symbol[length];

    for (size_t i = 0, s = 0; i < length; i++, s += 2) {
        symbol[i] = qpsk_mod(tx_bits, s);   // I Odd index, Q Even index
    }

    return qpsk_tx_frame(samples, symbol, length);
}

// Main Program

int main(int argc, char** argv) {
    int16_t frame[FRAME_SIZE];
    int length;

    srand(time(0));

    /*
     * Encode BPSK preamble
     *
     *           | 0  +1+j1
     *        ---+---
     * -1-j1   1 |
     *
     */
    for (size_t i = 0; i < PREAMBLE_LENGTH; i++) {
        float val = (float) preamblevalues[i];

        preambletable[i] = val + (val * I);
    }

    kalman_init();

    /*
     * Simulate the transmitted packets.
     */
    fout = fopen(TX_FILENAME, "wb");

    fbb_tx_phase = cmplx(0.0f);
    fbb_tx_rect = cmplx(TAU * CENTER / FS);

    uint8_t obits[(DATA_SYMBOLS * 2)];

    for (size_t k = 0; k < 10; k++) {
        // Send preamble unscrambled
        length = bpsk_modulate(frame);

        fwrite(frame, sizeof (int16_t), length, fout);

        //resetTXScrambler();
        
        /*
         * NS data frames between each preamble frame
         */
        for (size_t j = 0; j < NS; j++) {
            // 31 QPSK symbols scrambled
            
            for (size_t i = 0, s = 0; i < DATA_SYMBOLS; i++, s += 2) {
                uint8_t sdata = ((rand() % 2) << 1) | (rand() % 2);
                
                // no scramble for now                            TODO
                //sdata = scrambleTX(sdata);

                obits[s + 1] = (sdata >> 1) & 0x1;  // I Odd
                obits[s] = sdata & 0x1;             // Q Even
            }

            length = qpsk_modulate(frame, obits, DATA_SYMBOLS);

            fwrite(frame, sizeof (int16_t), length, fout);
        }

        // Dead space between packets
        
        int blank_frame[903] = { 0 };
        
        fwrite(blank_frame, sizeof (int16_t), 903, fout);    // some odd distance between packets
    }

    fclose(fout);

    /*
     * Now try to process what was transmitted
     */
    fin = fopen(TX_FILENAME, "rb");

    /*
     * Save the received bits.
     */
    fout = fopen(RX_FILENAME, "wb");

    fbb_rx_phase = cmplx(0.0f);
    fbb_rx_rect = cmplx(TAU * (-CENTER + FOFFSET) / FS);

    uint8_t ibits[BITS_PER_FRAME];

    state = hunt;

    while (1) {
        /*
         * Read in the frame samples
         */
        size_t count = fread(frame, sizeof (int16_t), FRAME_SIZE, fin);

        if (count != FRAME_SIZE)
            break;
        
        //resetRXScrambler();             TODO
        
        int valid = qpsk_rx_frame(frame, ibits);

        if (valid) {
            fwrite(ibits, sizeof (uint8_t), BITS_PER_FRAME, fout);
        }
    }

    fclose(fin);
    fclose(fout);

    return (EXIT_SUCCESS);
}
