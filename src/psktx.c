/*---------------------------------------------------------------------------*\

  FILE........: pskdv_tx.c
  AUTHORS.....: David Rowe & Steve Sampson
  DATE CREATED: November 2020

  A 1600 baud QPSK Digital Voice modem library

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

#include "pskdv_internal.h"
#include "fir.h"

// Externals

extern struct PSK *e_psk;
extern complex float e_pilots[];
extern complex float e_fcenter;
extern complex e_phaseTx;

// Prototypes

static void upconvert(complex float [], complex float[]);
static void clip(int16_t [], complex float [], int length);

// Locals

static complex float m_tx_filter[NTAPS];

/*
 * QPSK/BPSK Constellation - Gray Code
 */
static const complex float m_constellation[] = {
     1.0f + 0.0f * I,   // BPSK +1
     0.0f + 1.0f * I,
     0.0f - 1.0f * I,
    -1.0f + 0.0f * I    // BPSK -1
};

// Functions

static void clip(int16_t waveform[], complex float spectrum[], int length) {
    if (e_psk->clip == true) {
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

    // 32 PSK real data at 8 kHz sample rate

    for (int i = 0; i < length; i++) {
        waveform[i] = (int16_t) (crealf(spectrum[i]) / SCALE);
    }
}

/*
 * Function to produce PCM 16-bit 1-Channel waveform at 8 kHz rate.
 *
 * @param 1 BPSK pilot 1-Channel waveform
 */
void psk_pilot_modulate(int16_t waveform[]) {
    complex float tx_symb[PSK_SYMBOLS];

    for (int i = 0; i < PSK_SYMBOLS; i++) {
        tx_symb[i] = e_pilots[i];
    }
    
    // Interpolate by PSK_CYCLES

    int length = PSK_SYMBOLS * PSK_CYCLES;
    
    complex float spectrum[length];
    
    // create the QPSK modem frame (8 kHz rate)

    upconvert(spectrum, tx_symb);
    
    clip(waveform, spectrum, length);
}

/*
 * Function to produce PCM 16-bit 1-Channel waveform at 8 kHz rate.
 *
 * @param 1 QPSK data 1-Channel waveform
 * @param 2 bits int array of the data only bits (32 symbols * 2 bits) = 64 bits
 */
void psk_data_modulate(int16_t waveform[], int bits[]) {
    complex float tx_symb[PSK_SYMBOLS];

    for (int i = 0; i < PSK_SYMBOLS; i++) {
        // Sends IQ, IQ ... IQ or I = Odd Bits Q = Even Bits
        int bitPair = ((bits[(i * 2)] & 0x01) << 1) | (bits[(i * 2) + 1] & 0x01);

        tx_symb[i] = m_constellation[bitPair];
    }
    
    // Interpolate by PSK_CYCLES

    int length = PSK_SYMBOLS * PSK_CYCLES;
    
    complex float spectrum[length];
    
    // create the QPSK modem frame (8 kHz rate)

    upconvert(spectrum, tx_symb);
    
    clip(waveform, spectrum, length);
}

/*
 * Mix the BPSK or QPSK 32 symbol baseband signal up to center frequency.
 * 
 * @param waveform the complex output signal center frequency
 * @param baseband the input baseband signal to be mixed
 */
static void upconvert(complex float spectrum[], complex float baseband[]) {
    int length = (PSK_SYMBOLS * PSK_CYCLES);
    complex float signal[length];

    /*
     * Build the 1600 baud packet Frame zero padding
     * for the desired 8 kHz sample rate.
     */
    for (int i = 0; i < length; i++) {
        signal[(PSK_CYCLES * i)] = baseband[i];

        for (int j = 1; j < PSK_CYCLES; j++) {
            signal[(PSK_CYCLES * i) + j] = 0.0f;
        }
    }

    /*
     * Raised Root Cosine Filter
     */
    fir(m_tx_filter, signal, length);

    /*
     * Shift Baseband up to Center Frequency
     */
    for (int i = 0; i < length; i++) {
        e_phaseTx *= e_fcenter;
        spectrum[i] = signal[i] * e_phaseTx;
    }
    
    /* Normalize phase */
    e_phaseTx /= cabsf(e_phaseTx);
}
