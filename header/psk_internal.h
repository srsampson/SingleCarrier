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

#include <complex.h>
#include <stdint.h>
#include <math.h>

// Externally Accessible Data Elements
    
struct PSK {
    float m_freqEstimate;
    float m_freqFineEstimate;
    float m_signalRMS;
    float m_noiseRMS;
    float m_snrEstimate;
    int m_sync;
    int m_nin;
    int m_clip;
};

/*
 * Approximates pi accurately to about 8 decimal digits.
 * 
 * pi = 1.10010010000111111011011 x 2^1
 */
#ifndef M_PI
#define M_PI            3.1415927410125732421875f
#endif

#define TAU             (M_PI * 2.0f)
#define ROT45           (M_PI / 4.0f)

#define PSK_RS          1600.0f
#define PSK_FS          8000.0f
#define PSK_CENTER      1100.0f
#define PSK_CYCLES      (int) (PSK_FS / PSK_RS)
#define PSK_M           100

/*
 * This method is much faster than using cexp()
 * float_value - must be a float
 */
#define cmplx(float_value) (cosf(float_value) + sinf(float_value) * I)
#define cmplxconj(float_value) (cosf(float_value) + sinf(float_value) * -I)

#define PSK_SYMBOLS                  31
#define PSK_DATA_ROWS                7

// 7 rows * 31 QPSK symbols (217)
#define PSK_DATA_SYMBOLS_PER_FRAME   (PSK_SYMBOLS * PSK_DATA_ROWS)
    
// 1 row * 31 BPSK symbols
#define PSK_PILOT_SYMBOLS_PER_FRAME  PSK_SYMBOLS
    
// (7 rows * 31 QPSK symbols) * 2 bits (434)
#define PSK_DATA_BITS_PER_FRAME      (PSK_SYMBOLS * PSK_DATA_ROWS) * 2
    
// (1 rows * 31 BPSK symbols) * 1 bit (31)
#define PSK_PILOT_BITS_PER_FRAME     PSK_SYMBOLS

#define PSK_FRAME                    (PSK_PILOT_SYMBOLS_PER_FRAME + PSK_DATA_SYMBOLS_PER_FRAME)
#define PSK_SYMBOL_BUF               (PSK_FRAME + 2)

#define PSK_CLIP_AMP                 6.5f

#ifdef	__cplusplus
}
#endif
