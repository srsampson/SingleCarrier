/*---------------------------------------------------------------------------*\

  FILE........: qpsk.c
  AUTHORS.....: David Rowe & Steve Sampson
  DATE CREATED: October 2020

  A Dynamic Library of functions that implement a QPSK modem

\*---------------------------------------------------------------------------*/
/*
  Copyright (C) 2020 David Rowe

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2.1, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

// Includes

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <complex.h>
#include <string.h>

#include "qpsk_internal.h"
#include "fir.h"

// Prototypes

static float cnormf(complex float);
static float correlate_pilots(complex float [], int);
static float magnitude_pilots(complex float [], int);
static complex float qpsk_mod(int []);
static void qpsk_demod(complex float, int []);
static int tx_frame(int16_t [], complex float [], int, bool);

// Externals

extern const int8_t pilotvalues[];
extern const complex float constellation[];

// Globals

static State state;

static bool dpsk_en = false;

static complex float tx_filter[NTAPS];
static complex float rx_filter[NTAPS];

static complex float input_frame[(FRAME_SIZE * 2)];
static complex float decimated_frame[562]; // (FRAME_SIZE / CYCLES) * 2
static complex float pilot_table[PILOT_SYMBOLS];
static complex float rx_pilot[PILOT_SYMBOLS];

// Separate phase references for full duplex

static complex float fbb_tx_phase;
static complex float fbb_tx_rect;

static complex float fbb_rx_phase;
static complex float fbb_rx_rect;

static float rx_error;

// Functions

int create_qpsk_modem() {
    /*
     * Create a complex table of pilot values
     * for the correlation algorithm
     */
    for (int i = 0; i < PILOT_SYMBOLS; i++) {
        pilot_table[i] = (float) pilotvalues[i]; // complex -1.0 or 1.0
    }

    /*
     * Initialize center frequency and phase
     */
    fbb_tx_phase = cmplx(0.0f);
    fbb_rx_phase = cmplx(0.0f);

    fbb_tx_rect = cmplx(TAU * CENTER / FS);
    fbb_rx_rect = cmplx(TAU * -CENTER / FS);
}

int destroy_qpsk_modem() {
    // TODO
}

/*
 * For use when sqrt() is not needed
 */
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
 * Return magnitude of symbols (sans sqrt)
 */
static float magnitude_pilots(complex float symbol[], int index) {
    float out = 0.0f;

    for (int i = index; i < (PILOT_SYMBOLS + index); i++) {
        out += cnormf(symbol[i]);
    }
    
    return out;
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
static complex float qpsk_mod(int bits[]) {
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
 * By transmitting I+Q at transmitter, the signal has a
 * 45 degree shift, which is just what we need to find
 * the quadrant the symbol is in.
 * 
 * Each bit pair differs from the next by only one bit.
 */
static void qpsk_demod(complex float symbol, int bits[]) {
    bits[0] = crealf(symbol) < 0.0f; // I < 0 ?
    bits[1] = cimagf(symbol) < 0.0f; // Q < 0 ?
}
/*
 * Receive function
 * 
 * Process a 1600 baud QPSK at 8000 samples/sec.
 * 
 * Each frame is made up of 33 Pilots and 31 x 8 Data symbols.
 * This is (33 * 5) = 165 + (31 * 5 * 8) = 1240 or 1405 samples per packet
 */
void qpsk_rx_frame(int16_t in[], uint8_t bits[]) {
    complex float fourth = (1.0f / 4.0f);
    int timing_offset = FINE_TIMING_OFFSET; // use pre-computed estimate

    /*
     * Convert input PCM to complex samples
     * Translate to Baseband at an 8 kHz sample rate
     */
    for (int i = 0; i < FRAME_SIZE; i++) {
        fbb_rx_phase *= fbb_rx_rect;

        complex float val = fbb_rx_phase * ((float) in[i] / 16384.0f);

        input_frame[i] = input_frame[FRAME_SIZE + i];
        input_frame[FRAME_SIZE + i] = val;
    }

    fbb_rx_phase /= cabsf(fbb_rx_phase); // normalize as magnitude can drift

    /*
     * Raised Root Cosine Filter at Baseband
     */
    fir(rx_filter, input_frame, FRAME_SIZE);

    /*
     * Decimate by 5 to the 1600 symbol rate
     */
    for (int i = 0; i < (FRAME_SIZE / CYCLES); i++) {
        decimated_frame[i] = decimated_frame[(FRAME_SIZE / CYCLES) + i];
        decimated_frame[(FRAME_SIZE / CYCLES) + i] = input_frame[(i * CYCLES) + timing_offset];

        /*
         * Compute the QPSK phase error
         */
        float phase_error = cargf(cpowf(decimated_frame[(FRAME_SIZE / CYCLES) + i], 4.0f) * fourth); // division is slow

        timing_offset = (int) roundf(fabsf(phase_error)); // only positive
    }

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

    if (max_value > (mean * 30.0f)) {
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

        // qpsk_demod();          // TODO
        
    } else {
        /*
         * Burn remainder of frame
         */
        state = hunt;
    }
}


void qpsk_rx_offset(float fshift) {
    fbb_rx_rect *= cmplx(TAU * fshift / FS);
}

/*
 * Dead man switch to state end
 */
void qpsk_rx_end() {
    state = hunt;
}

/*
 * Modulate the symbols by first upsampling to 8 kHz sample rate,
 * and translating the spectrum to 1100 Hz, where it is filtered
 * using the root raised cosine coefficients.
 * 
 * These can be either a Pilot or Data Frame
 */
static int tx_frame(int16_t frame[], complex float symbol[], int length, bool dpsk) {
    complex float signal[(length * CYCLES)];

    /*
     * Build the 1600 baud packet Frame zero padding
     * for the desired 8 kHz sample rate.
     */
    if (dpsk == true) {
        //TODO
        for (int i = 0; i < length; i++) {
            signal[(i * CYCLES)] = symbol[i];

            for (int j = 1; j < CYCLES; j++) {
                signal[(i * CYCLES) + j] = 0.0f;
            }
        }
    } else {
        for (int i = 0; i < length; i++) {
            signal[(i * CYCLES)] = symbol[i];

            for (int j = 1; j < CYCLES; j++) {
                signal[(i * CYCLES) + j] = 0.0f;
            }
        }
    }
    
    /*
     * Root Cosine Filter at Baseband
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
     * Now return the resulting I+Q
     * Note: Summation results in 45 deg phase shift
     */
    for (int i = 0; i < (length * CYCLES); i++) {
        frame[i] = (int16_t) ((crealf(signal[i]) + cimagf(signal[i])) * 16384.0f); // I at @ .5
    }
    
    return (length * CYCLES);
}

int qpsk_get_number_of_pilot_bits() {
    return PILOT_SYMBOLS;
}

int qpsk_get_number_of_data_bits() {
    return (DATA_SYMBOLS * 2);
}

/*
 * Return the length in symbols of a frame of PCM symbols at the Center Frequency
 * The modulation data is assumed to be fixed pilot bits
 */
int qpsk_pilot_modulate(int16_t frame[]) {
    return tx_frame(frame, pilot_table, PILOT_SYMBOLS, false);
}

/*
 * Return the length in symbols of a frame of PCM symbols at the Center Frequency
 * The modulation data is given as unsigned character bits
 * The index is used to avoid pointers, and is a reference into array bits.
 */
int qpsk_data_modulate(int16_t frame[], uint8_t bits[], int index) {
    complex float symbol[DATA_SYMBOLS];
    int dibit[2];

    for (int i = 0, s = index; i < DATA_SYMBOLS; i++, s += 2) {
        dibit[0] = bits[s + 1] & 0x1;
        dibit[1] = bits[s ] & 0x1;

        symbol[i] = qpsk_mod(dibit);
    }

    return tx_frame(frame, symbol, DATA_SYMBOLS, dpsk_en);
}
