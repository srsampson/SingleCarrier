/*---------------------------------------------------------------------------*\
  FILE........: psk_internal.h
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

#pragma once

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <complex.h>

#define NORM_PWR        1.74f
#define MODEM_SCALE     1000.0f

/*
 * Approximates pi accurately to about 8 decimal digits.
 * 
 * pi = 1.10010010000111111011011 x 2^1
 */
#ifndef M_PI
#define M_PI                         3.1415927410125732421875f
#endif

#define TAU                          (M_PI * 2.0f)

    /*
 * This method is much faster than using cexp()
 * float_value - must be a float
 */
#define cmplx(float_value) (cosf(float_value) + sinf(float_value) * I)
#define cmplxconj(float_value) (cosf(float_value) + sinf(float_value) * -I)

#define ROT45                        (M_PI / 4.0f)

#define PSK_NOM_TX_SAMPLES_PER_FRAME 600
#define PSK_NOM_RX_SAMPLES_PER_FRAME 640
#define PSK_BITS_PER_FRAME           56

#define PSK_CLIP                     6.5f
#define PSK_M                        100
#define PSK_NSYM                     6
#define PSK_NFILTER                  (PSK_NSYM * PSK_M)
#define PSK_RS                       1600
#define PSK_FS                       8000.0f
#define PSK_CENTER                   1100.0f
#define NT                           5
#define P                            4
#define DATA_SYMBOLS                 31
#define PILOT_SYMBOLS                33
#define NSYMPILOTDATA                (PILOT_SYMBOLS + DATA_SYMBOLS) 
#define NCT_SYMB_BUF                 (NSYMPILOTDATA + 2)
#define NSW                          4
#define GAIN                         (sqrtf(2.0f) / 2.0f)

#define TAPS                         49

/* Data Elements combined into a shared memory structure */
    
struct PSK {
    complex float m_txFilterMemory[PSK_NSYM];
    complex float m_rxFilterMemory[PSK_NFILTER];
    complex float m_rxFilterMemTiming[NT * P];
    complex float m_carrier;
    complex float m_phaseTx;
    complex float m_phaseRx;
    complex float m_fbbPhaseTx;
    complex float m_fbbPhaseRx;
    complex float m_rxsamples[TAPS];
    complex float m_txsamples[TAPS];
    complex float m_crxsamples[PSK_NOM_RX_SAMPLES_PER_FRAME];
    complex float m_ctxsamples[PSK_NOM_TX_SAMPLES_PER_FRAME];
    complex float m_rxSymb[DATA_SYMBOLS];
    complex float m_ctSymbBuf[NCT_SYMB_BUF];
    complex float m_ctFrameBuf[NSYMPILOTDATA + 2];
    complex float m_chFrameBuf[NSW * NSYMPILOTDATA * PSK_M];
    complex float m_prevRxSymbols;
    float m_pilot2[PILOT_SYMBOLS * 2];
    float m_pskPhase[DATA_SYMBOLS];
    float m_freqEstimate;
    float m_ratio;
    float m_signalRMS;
    float m_noiseRMS;
    float m_snrEstimate;
    float m_freqOffsetFiltered;
    float m_rxTiming;
    float m_freqFineEstimate;
    bool m_sync;
    int m_sampleCenter;
    int m_nin;
    int m_syncTimer;
};

#ifdef	__cplusplus
}
#endif

