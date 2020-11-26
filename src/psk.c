/*---------------------------------------------------------------------------*\

  FILE........: psk.c
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

#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "psk_internal.h"

// Globals

const char copyright[] = "Copyright (C) 2020 David Rowe\r\nGNU LGPL 2.1 License\r\n";

/*
 * QPSK Constellation - Gray Code
 * 
 *         Q 01
 *         |
 * 11 -I---+---I 00
 *         |
 *        -Q 10
 * 
 * The symbols are not rotated on transmit
 */
const complex float constellation[] = {
     1.0f + 0.0f * I,
     0.0f + 1.0f * I,
     0.0f - 1.0f * I,
    -1.0f + 0.0f * I
};

/*
 * Same number of pilots as data symbols
 */
const int8_t pilots[] = {
    -1,  1,  1, -1, -1, -1,  1, -1,
     1, -1,  1,  1,  1,  1,  1,  1,
     1,  1, -1, -1,  1, -1,  1, -1,
     1,  1,  1,  1,  1,  1, -1
};

/*
 * Linear Regression X point values.
 * 0,1 for start, and 29,30 for end.
 * Algorithm will fit the rest.
 */
const int samplingPoints[] = {
    0, 1, 29, 30
};

struct PSK *psk;

// Functions

int psk_create() {

    psk = (struct PSK *) calloc(1, sizeof(struct PSK));
    
    if (psk == NULL)
        return 1;

    /* Initialize Transmit and Receive Side */

    psk->m_carrier = cmplx(TAU * PSK_CENTER / PSK_FS);
    
    psk->m_phaseTx = cmplx(0.0f);
    psk->m_phaseRx = cmplx(0.0f);

    /*
     * Initialize the pilot phases
     * Same number of pilots as data
     */
    for (int i = 0; i < MOD_SYMBOLS; i++) {
        psk->m_pilots[i] = (float) pilots[i] + 0.0 * I; // I + j0
    }

    psk->m_nin = PSK_M;
    psk->m_clip = 1;    // clip TX waveform

    return 0;
}

void psk_destroy() {
    free(psk);
}

/*
 * Provide an API for reading the modem sync frequency.
 * 
 * It's the users responsibility to check for sync, but
 * they may want to know the value even if not in sync.
 */
float psk_get_frequency_estimate() {
    return psk->m_freqEstimate;
}

/*
 * Provide an API for reading the modem sync frequency
 * of the pilot calculated fine frequency center.
 * 
 * It's the users responsibility to check for sync, but
 * they may want to know the value even if not in sync.
 */
float psk_get_fine_frequency_estimate() {
    return psk->m_freqFineEstimate;
}

/*
 * After returning the SNR to the caller,
 * the values are filtered for the next call,
 * just to smooth the values over time.
 */
float psk_get_SNR() {
    float new_snr_est = 20.0f * log10f((psk->m_signalRMS + 1E-6f)
            / (psk->m_noiseRMS + 1E-6f)) - 10.0f * log10f(3000.0f / 2400.0f);

    psk->m_snrEstimate = 0.9f * psk->m_snrEstimate + 0.1f * new_snr_est;

    return psk->m_snrEstimate;
}

/*
 * Return the sync state to the caller
 * 0 = false, 1 = true
 */
int psk_get_SYNC() {
    return psk->m_sync;
}

int psk_get_NIN() {
    return psk->m_nin;
}

int psk_get_clip() {
    return psk->m_clip;
}

void psk_set_clip(int val) {
    psk->m_clip = (val != 0);
}
