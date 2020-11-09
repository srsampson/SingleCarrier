/*---------------------------------------------------------------------------*\

  FILE........: fir.c
  AUTHORS.....: David Rowe & Steve Sampson
  DATE CREATED: October 2020

  A FIR filter used in the QPSK modem

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

#include "qpsk_internal.h"
#include "fir.h"

// Externals

extern const float alpha35_root[];

/*
 * FIR Filter with specified impulse length used at 8 kHz
 */
void fir(complex float memory[], complex float sample[], int length) {
    for (int j = 0; j < length; j++) {
        for (int i = 0; i < (NTAPS - 1); i++) {
            memory[i] = memory[i + 1];
        }

        memory[(NTAPS - 1)] = sample[j];

        complex float y = 0.0f;

        for (int i = 0; i < NTAPS; i++) {
            y += (memory[i] * alpha35_root[i]);
        }

        sample[j] = y * GAIN;
    }
}