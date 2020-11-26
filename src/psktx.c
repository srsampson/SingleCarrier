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
extern complex float pilots[];
extern const complex float constellation[];
extern complex float fcenter;
extern complex phaseTx;

// Prototypes

static void upconvert(complex float [], complex float[]);
static void bitsToConstellation(complex float [], int []);

// Locals

static complex float tx_filter[NTAPS];

// Functions

/*
 * Function to return an IQ PCM 16-bit 2-Channel waveform at 8 kHz rate.
 *
 * @param waveform complex array of the modulated IQ data and pilots frame
 * @param bits int array of the data only bits (7 * 31 * 2) = 434 bits
 * @return int the number of complex IQ samples (2480 16-bit PCM samples)
 */
int psk_modulate(int16_t waveform[], int bits[]) {
    complex float tx_symb[PSK_FRAME];

    /*
     * tx_symb will have the constellation symbols
     * as determined by the bit pairs at 1600 baud
     */
    bitsToConstellation(tx_symb, bits);

    // length increased now by PSK_CYCLES

    int length = PSK_FRAME * PSK_CYCLES;
    
    complex float spectrum[length];
    
    // create the PSK modem frame (8 kHz rate)

    upconvert(spectrum, tx_symb);
    
    if (psk->m_clip) {
        /*
         * Reduce Crest Factor by about 2 dB
         * this will typically occur about 5% of the signal samples
         */
        for (int i = 0; i < length; i++) {
            float mag = cabsf(spectrum[i]);

            if (mag > PSK_CLIP_AMP) { /* 6.5 */
                spectrum[i] *= (PSK_CLIP_AMP / mag);
            }
        }
    }

    // (7 rows * 31 QPSK data + 31 BPSK pilots) = 248 symbols at 1600 Hz
    // Multiply by 5 CYCLES to get 8 kHz rate = 1240 IQ samples
    // 2-Channel IQ PCM 16-bit is then sent (2480)

    for (int i = 0, j = 0; i < length; i++, j += 2) {
        waveform[j] = (int16_t) (crealf(spectrum[i]) / SCALE);
        waveform[j+1] = (int16_t) (cimagf(spectrum[i]) / SCALE);
    }

    // We now have 1240 * 2 = 2480 for 2-Channel PCM 16-bit

    return length * 2;
}

/*
 * QPSK Modulate the data and pilot bits
 * 
 * @param symbols the modulated pilot and data bits
 * @param bits the data bits to be modulated
 */
static void bitsToConstellation(complex float symbols[], int bits[]) {
    // First do the pilot symbols...

    for (int i = 0; i < PSK_PILOT_SYMBOLS_PER_FRAME; i++) {
        symbols[i] = pilots[i];
    }

    // ...then add the data symbols

    for (int i = 0, j = PSK_PILOT_SYMBOLS_PER_FRAME;
            i < PSK_DATA_SYMBOLS_PER_FRAME; i++, j++) {
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
static void upconvert(complex float spectrum[], complex float baseband[]) {
    int length = (PSK_FRAME * PSK_CYCLES);
    complex float signal[length];

    /*
     * Build the 1600 baud packet Frame zero padding
     * for the desired 8 kHz sample rate.
     */
    for (int i = 0; i < PSK_FRAME; i++) {
        signal[(PSK_CYCLES * i)] = baseband[i];

        for (int j = 1; j < PSK_CYCLES; j++) {
            signal[(PSK_CYCLES * i) + j] = 0.0f;
        }
    }

    /*
     * Raised Root Cosine Filter
     */
    fir(tx_filter, signal, length);

    /*
     * Shift Baseband up to Center Frequency
     */
    for (int i = 0; i < length; i++) {
        phaseTx *= fcenter;
        spectrum[i] = signal[i] * phaseTx;
    }
    
    /* Normalize phase */
    phaseTx /= cabsf(phaseTx);
}
