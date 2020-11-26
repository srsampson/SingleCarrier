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
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

#include "psk_internal.h"

// Globals

struct PSK *psk;
const char copyright[] = "Copyright (C) 2020 David Rowe\r\nGNU LGPL 2.1 License\r\n";
complex float fcenter;
complex float pilots[PSK_PILOT_SYMBOLS_PER_FRAME];
complex phaseRx;
complex phaseTx;

/*
 * QPSK Constellation - Gray Code
 */
const complex float constellation[] = {
     1.0f + 0.0f * I,
     0.0f + 1.0f * I,
     0.0f - 1.0f * I,
    -1.0f + 0.0f * I
};

// Locals

/*
 * Same number of pilots as data symbols
 */
static const int8_t pilotvalues[] = {
    -1,  1,  1, -1, -1, -1,  1, -1,
     1, -1,  1,  1,  1,  1,  1,  1,
     1,  1, -1, -1,  1, -1,  1, -1,
     1,  1,  1,  1,  1,  1, -1
};

// Functions

int psk_create() {
    // Initialize to all zero
    if ((psk = (struct PSK *) calloc(1, sizeof(struct PSK))) == NULL) {
        // Return failure
        return 0;
    }

    // Initialize Transmit and Receive phases

    fcenter = cmplx(TAU * PSK_CENTER / PSK_FS);
    
    phaseTx = cmplx(0.0f);
    phaseRx = cmplx(0.0f);

    /*
     * Initialize the pilot phases
     * Same number of pilots as data
     */
    for (int i = 0; i < PSK_SYMBOLS; i++) {
        pilots[i] = (float) pilotvalues[i] + 0.0 * I; // I + j0
    }

    psk->m_nin = PSK_CYCLES;
    psk->m_clip = 1;    // clip TX waveform

    // Return success
    return 1;
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
