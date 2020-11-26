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
#include "fir.h"

// Externals

extern struct PSK *psk;
extern const float pilots[MOD_SYMBOLS];
extern const complex float constellation[];
extern const float alpha50_root[];

// Prototypes

static void upconvert(complex float [], complex float);
static void bitsToConstellation(complex float [], int []);

// Functions

/*
 * Function to return a modulated frame from the bits provided.
 *
 * @param symbols complex array of the modulated frame (31 + (7 * 31) 248 symbols
 * @param bits int array of the data bits (496 bits)
 * @return int the number of symbols to be processed
 */
int psk_modulate(complex float symbols[], int bits[]) {
    complex float tx_symb[PILOTDATA_SYMBOLS];

    /*
     * tx_symb will have the constellation symbols
     * as determined by the bit pairs at 1600 baud
     */
    bitsToConstellation(tx_symb, bits);

    /* create the PSK modem frame (8 kHz rate) */

    upconvert(symbols, tx_symb);

    if (psk->m_clip == 1) {
        /*
         * Reduce Crest Factor by about 2 dB
         * this will typically occur about 5% of the signal samples
         */
        for (int i = 0; i < PSK_SYMBOLS_PER_FRAME; i++) {
            float mag = cabsf(symbols[i]);

            if (mag > PSK_CLIP) { /* 6.5 */
                symbols[i] *= (PSK_CLIP / mag);
            }
        }
    }

    /* Amplify the signal to user-level in the complex array */
        
    for (int i = 0; i < PSK_SYMBOLS_PER_FRAME; i++) {
        symbols[i] *= (MODEM_SCALE * NORM_PWR);
    }

    return PSK_SYMBOLS_PER_FRAME;
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

    for (int i = 0; i < MOD_SYMBOLS; i++) {
        symbols[i] = psk->m_pilots[i];
    }

    /* ...then add the data symbols */

    for (int i = 0, j = MOD_SYMBOLS; i < MOD_SYMBOLS; i++, j++) {
        // Sends IQ, IQ ... IQ or I = Odd Bits Q = Even Bits
        int bitPair = ((bits[(i * 2)] & 0x01) << 1) | (bits[(i * 2) + 1] & 0x01);

        symbols[j] = constellation[bitPair];
    }
}

/*
 * Mix the baseband signal into the output waveform.
 * 
 * @param waveform the complex output signal centered on freq
 * @param baseband the input baseband signal to be mixed
 */
static void upconvert(complex float waveform[], complex float baseband) {
    complex float signal[(PILOTDATA_SYMBOLS * PSK_CYCLES)];

    /*
     * Build the 1600 baud packet Frame zero padding
     * for the desired 8 kHz sample rate.
     */
    for (int i = 0; i < PILOTDATA_SYMBOLS; i++) {
        signal[(i * PSK_CYCLES)] = baseband[i];

        for (int j = 1; j < PSK_CYCLES; j++) {
            signal[(i * PSK_CYCLES) + j] = 0.0f;
        }
    }

    /*
     * Raised Root Cosine Filter
     */
    fir(psk->m_tx_filter, signal, (PILOTDATA_SYMBOLS * PSK_CYCLES));

    /*
     * Shift Baseband up to Center Frequency
     */
    for (int i = 0; i < (PILOTDATA_SYMBOLS * PSK_CYCLES); i++) {
        psk->m_phaseTx *= psk->m_carrier;
        waveform[i] = signal[i] * psk->m_phaseTx;
    }
    
    /* Normalize phase */
    psk->m_phaseTx /= cabsf(psk->m_phaseTx);
}
