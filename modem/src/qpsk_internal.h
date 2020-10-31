/*---------------------------------------------------------------------------*\

  FILE........: qpsk_internal.h
  AUTHORS.....: David Rowe & Steve Sampson
  DATE CREATED: October 2020

  A Library of functions that implement a QPSK modem

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

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <complex.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h> 

/* manual/hard coded fine timing estimation for now */
#define FINE_TIMING_OFFSET 3

#define FS              8000.0f
#define RS              1600.0f
#define CYCLES          (int) (FS / RS)
#define CENTER          1100.0f

#define NS              8
#define PILOT_SYMBOLS   33
#define DATA_SYMBOLS    31
#define FRAME_SIZE      1405

// (DATA_SYMBOLS * 2 bits * NS)
#define BITS_PER_FRAME  496

#ifndef M_PI
#define M_PI            3.14159265358979323846f
#endif

#define TAU             (2.0f * M_PI)
#define ROT45           (M_PI / 4.0f)

/*
 * This method is much faster than using cexp() when real == 0
 * value - must be a float
 */
#define cmplx(value) (cosf(value) + sinf(value) * I)
#define cmplxconj(value) (cosf(value) + sinf(value) * -I)

typedef struct
{
    int data;
    int tx_symb;
    float cost;
    complex float rx_symb;
} Rxed;

/* modem state machine states */

typedef enum
{
    hunt,
    process
} State;

#ifdef __cplusplus
}
#endif
