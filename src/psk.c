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
#include <stdbool.h>
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

const int8_t pilots[PILOT_SYMBOLS] = {
    -1, -1, 1, 1, -1, -1, -1, 1,
    -1, 1, -1, 1, 1, 1, 1, 1,
    1, 1, 1, -1, -1, 1, -1, 1,
    -1, 1, 1, 1, 1, 1, 1, 1, 1
};

/*
 * Linear Regression X point values.
 * 0,1 for start, and 31,32 for end.
 * Algorithm will fit the rest.
 */
const int samplingPoints[] = {
    0, 1, 31, 32
};

/*
 * Cosine on a pedestal FIR filter
 * coefficients designed for .5 alpha
 *
 * Created with Octave:
 * hs = gen_rn_coeffs(.5, 1.0/8000.0, 1600, 10, 5);
 */
const float gtAlpha5Root[] = {
    0.002040776f,
    0.001733205f,
    -0.000094696f,
    -0.002190566f,
    -0.002803057f,
    -0.001145122f,
    0.001875377f,
    0.004037490f,
    0.003421695f,
    0.000028693f,
    -0.003768086f,
    -0.004657093f,
    -0.000932888f,
    0.005513738f,
    0.009520251f,
    0.005665029f,
    -0.007427566f,
    -0.024194919f,
    -0.032975574f,
    -0.021014393f,
    0.018508466f,
    0.081140162f,
    0.150832112f,
    0.205501104f,
    0.226202985f,
    0.205501104f,
    0.150832112f,
    0.081140162f,
    0.018508466f,
    -0.021014393f,
    -0.032975574f,
    -0.024194919f,
    -0.007427566f,
    0.005665029f,
    0.009520251f,
    0.005513738f,
    -0.000932888f,
    -0.004657093f,
    -0.003768086f,
    0.000028693f,
    0.003421695f,
    0.004037490f,
    0.001875377f,
    -0.001145122f,
    -0.002803057f,
    -0.002190566f,
    -0.000094696f,
    0.001733205f,
    0.002040776f
};

struct PSK *psk;

// Functions

int pskCreate() {

    psk = (struct PSK *) malloc(sizeof(struct PSK));
    
    /* Initialize Transmit and Receive Side */

    psk->m_carrier = cmplx(TAU * PSK_CENTER / PSK_FS);
    
    psk->m_phaseTx = cmplx(0.0f);
    psk->m_phaseRx = cmplx(0.0f);

    psk->m_fbbPhaseTx = cmplx(0.0f);
    psk->m_fbbPhaseRx = cmplx(0.0f);

    /*
     * Initialize the pilot phases
     */
    for (int i = 0; i < PILOT_SYMBOLS; i++) {
        psk->m_pilot2[i] = (float) pilots[i];
    }

    psk->m_nin = PSK_M;

    return 0;
}

void pskDestroy() {
    free(psk);
}

/*
 * Provide an API for reading the modem sync frequency.
 * 
 * It's the users responsibility to check for sync, but
 * they may want to know the value even if not in sync.
 */
float getCenterFrequency() {
    return psk->m_freqEstimate;
}

/*
 * Provide an API for reading the modem sync frequency
 * of the pilot calculated fine center.
 * 
 * It's the users responsibility to check for sync, but
 * they may want to know the value even if not in sync.
 */
float getFrequencyFineEstimate() {
    return psk->m_freqFineEstimate;
}

/*
 * After returning the SNR to the caller,
 * the values are filtered for the next call,
 * just to smooth the values over time.
 */
float getSNR() {
    float new_snr_est = 20.0f * log10f((psk->m_signalRMS + 1E-6f)
            / (psk->m_noiseRMS + 1E-6f)) - 10.0f * log10f(3000.0f / 2400.0f);

    psk->m_snrEstimate = 0.9f * psk->m_snrEstimate + 0.1f * new_snr_est;

    return psk->m_snrEstimate;
}

/*
 * Return the sync state to the caller
 */
bool getSync() {
    return psk->m_sync;
}

int getNIN() {
    return psk->m_nin;
}

/* EOF */
