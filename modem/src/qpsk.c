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

static complex float qpsk_mod(int []);
static int tx_frame(int16_t [], complex float [], int, bool);

// Externals

extern const int8_t pilotvalues[];
extern const complex float constellation[];

// Globals

static bool dpsk_en = false;

static complex float tx_filter[NTAPS];
static complex float pilot_table[PILOT_SYMBOLS];
static int16_t frame[FRAME_SIZE];
static uint8_t bits[BITS_PER_FRAME];

static complex float fbb_tx_phase;
static complex float fbb_tx_rect;

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
    fbb_tx_rect = cmplx(TAU * CENTER / FS);
}

int destroy_qpsk_modem() {
    // TODO
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
