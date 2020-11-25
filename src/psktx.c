/*---------------------------------------------------------------------------*\
  FILE........: psktx.c
  AUTHORS.....: David Rowe & Steve Sampson
  DATE CREATED: November 2020

  A 1600 baud QPSK voice modem library
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

#include "psk_internal.h"

// Externals

extern struct PSK *psk;
extern const float pilots[PILOT_SYMBOLS];
extern const complex float constellation[];
extern const float gtAlpha5Root[];

// Prototypes

static void upconvert(complex float [], int, complex float);
static void bitsToConstellation(complex float [], int []);

// Functions

/*
 * Function to return a modulated signal of the bits provided.
 *
 * @param symbols a complex array of the modulated signal
 * @param bits a boolean array of the 56 data bits
 * @return int the number of symbols to be processed
 */
int modulate(complex float symbols[], int bits[]) {
    complex float tx_symb[NSYMPILOTDATA];

    /*
     * tx_symb will have the constellation symbols
     * as determined by the bit pairs
     */
    bitsToConstellation(tx_symb, bits);

    /* create the PSK modem frame */

    for (int i = 0; i < NSYMPILOTDATA; i++) {
        upconvert(symbols, (i * PSK_M), tx_symb[i]);
    }

    /*
     * Reduce Crest Factor by about 2 dB
     * this will typically occur about 5% of the signal samples
     */
    for (int i = 0; i < PSK_NOM_TX_SAMPLES_PER_FRAME; i++) {
        float mag = cabsf(symbols[i]);

        if (mag > PSK_CLIP) {                /* 6.5 */
            symbols[i] *= (PSK_CLIP / mag);
        }
    }
    
    /* Amplify the signal to user-level in the complex array */
        
    for (int i = 0; i < PSK_NOM_TX_SAMPLES_PER_FRAME; i++) {
        symbols[i] *= (MODEM_SCALE * NORM_PWR);
    }

    return PSK_NOM_TX_SAMPLES_PER_FRAME;
}

/*
 * QPSK Modulate the data and pilot bits
 * 
 * @param symbols the modulated pilot and data bits
 * @param bits the data bits to be modulated
 */
static void bitsToConstellation(complex float symbols[], int bits[]) {
    /*
     * Insert pilots at beginning of frame.
     * Insert data after the pilots.
     */

    /* First do the pilot symbols... */

    for (int i = 0; i < PILOT_SYMBOLS; i++) {
        symbols[i] = (complex float) pilots[i];
    }

    /* ...then do the data symbols */

    for (int i = 0, j = PILOT_SYMBOLS; i < DATA_SYMBOLS; i++, j++) {
        int bitPair = ((bits[(i * 2)] & 0x01) << 1) | (bits[(i * 2) + 1] & 0x01);

        symbols[j] = constellation[bitPair];
    }
}

/*
 * Mix the new baseband signal segment into the output waveform.
 * 
 * Using an offset variable is a little cleaner than just offsetting
 * the waveform pointer, which is not very self-documenting.
 * 
 * @param waveform the complex output signal centered on freq
 * @param offset the index to put the mixed signal in the waveform array
 * @param baseband the input baseband signal to be mixed
 */
static void upconvert(complex float waveform[], int offset, complex float baseband) {
    /* Clean out the buffer */

    for (int i = 0; i < PSK_M; i++) {
        waveform[offset + i] = 0.0f;    // complex
    }

    /*
     * Add the (baseband symbols * .707 amplitude) to the last row,
     * which is available now, since the earlier call has
     * moved each row up one row.
     */
    psk->m_txFilterMemory[PSK_NSYM - 1] = baseband * GAIN; /* bottom row */

    /*
     * Run the filter over the baseband constellation for each row
     */
    for (int i = 0; i < PSK_M; i++) {
        complex float acc = 0.0f;
// TODO
        for (int j = 0, k = 0; j < PSK_NSYM; j++, k++) {
            acc += PSK_M * psk->m_txFilterMemory[j] * gtAlpha5Root[k];
        }

        /* Adjust the baseband phase and add symbol */

        psk->m_phaseTx *= psk->m_carrier;
        waveform[offset + i] += (acc * psk->m_phaseTx);
    }

    /* Adjust the final phase, and move the baseband segment up to freq */

    complex float temp = cmplx(TAU * PSK_CENTER / PSK_FS);

    for (int i = 0; i < PSK_M; i++) {
        psk->m_fbbPhaseTx *= temp;
        waveform[offset + i] *= (psk->m_fbbPhaseTx * 2.0f);
    }

    /* Normalize carriers of the baseband phase */

    psk->m_phaseTx /= cabsf(psk->m_phaseTx);

    /* same for up-conversion phase */

    psk->m_fbbPhaseTx /= cabsf(psk->m_fbbPhaseTx);

    /*
     * Move the rows 1..5 up one older position (in time). You don't
     * have to worry about zero'ing out the last row, as the new rows
     * carrier symbols will go there on the next call to this function.
     */
    for (int i = 0; i < (PSK_NSYM - 1); i++) {
        psk->m_txFilterMemory[i] = psk->m_txFilterMemory[i + 1];
    }
}

/* EOF */