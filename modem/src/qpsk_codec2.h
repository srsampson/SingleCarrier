/*---------------------------------------------------------------------------*\

  FILE........: qpsk_codec2.h
  AUTHORS.....: David Rowe & Steve Sampson
  DATE CREATED: October 2020

  A Dynamic Library include header for a QPSK modem

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

#include <complex.h>
#include <stdint.h>

// Prototypes

int qpsk_create(void);
int qpsk_destroy(void);

int qpsk_pilot_modulate(int16_t []);
int qpsk_data_modulate(int16_t [], uint8_t [], int);

int qpsk_get_number_of_pilot_bits(void);
int qpsk_get_number_of_data_bits(void);

void qpsk_rx_freq_shift(complex float [], complex float [], int, int, float, complex float);
void qpsk_rx_frame(int16_t [], uint8_t []);
void qpsk_rx_end(void);

#ifdef __cplusplus
}
#endif
