/*---------------------------------------------------------------------------*\

  FILE........: pskdv.c
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

#include "pskdv_internal.h"

// Globals

struct PSK *e_psk;

complex float e_fcenter;
complex float e_pilots[PSK_PILOT_SYMBOLS_PER_FRAME];
complex e_phaseRx;
complex e_phaseTx;

// Locals

/*
 * Same number of pilots (32) as data symbols
 */
static const int8_t pilotvalues[] = {
    -1,  1,  1, -1, -1, -1,  1, -1,
     1, -1,  1,  1,  1,  1,  1,  1,
     1,  1, -1, -1,  1, -1,  1, -1,
     1,  1,  1,  1,  1,  1,  1, -1
};

const char copyright[] = "Copyright (C) 2020 David Rowe\r\nGNU LGPL 2.1 License\r\n";

// Functions

int psk_create() {
    // Initialize to all zero
    if ((e_psk = (struct PSK *) calloc(1, sizeof(struct PSK))) == NULL) {
        // Return failure
        return 0;
    }

    // Initialize Transmit and Receive phases

    e_fcenter = cmplx(TAU * PSK_CENTER / PSK_FS);
    
    e_phaseTx = cmplx(0.0f);
    e_phaseRx = cmplx(0.0f);

    /*
     * Initialize the pilot phases
     * Same number of pilots (32) as data
     */
    for (int i = 0; i < PSK_SYMBOLS; i++) {
        e_pilots[i] = (float) pilotvalues[i] + 0.0 * I; // I + j0
    }
    
    e_psk->nin = PSK_CYCLES;
    e_psk->clip = true;    // clip TX waveform

    // Return success
    return 1;
}

void psk_destroy() {
    free(e_psk);
}

/*
 * Provide an API for reading the modem sync frequency.
 * 
 * It's the users responsibility to check for sync, but
 * they may want to know the value even if not in sync.
 */
float psk_get_frequency_estimate() {
    return e_psk->freqEstimate;
}

/*
 * Provide an API for reading the modem sync frequency
 * of the pilot calculated fine frequency center.
 * 
 * It's the users responsibility to check for sync, but
 * they may want to know the value even if not in sync.
 */
float psk_get_fine_frequency_estimate() {
    return e_psk->freqFineEstimate;
}

/*
 * SNR of last receive frame
 */
float psk_get_SNR() {
    return e_psk->snrEstimate;
}

/*
 * Return the sync state to the caller
 * 0 = false, 1 = true
 */
int psk_get_SYNC() {
    return e_psk->sync;
}

int psk_get_NIN() {
    return e_psk->nin;
}

bool psk_get_clip() {
    return e_psk->clip;
}

/*
 * User clipping toggle switch
 */
void psk_set_clip(bool val) {
    e_psk->clip = val;
}
