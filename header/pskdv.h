/*---------------------------------------------------------------------------*\
  
  FILE........: pskdv.h
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

#pragma once

#ifdef	__cplusplus
extern "C" {
#endif

/* Includes */

#include <complex.h>
#include <stdint.h>

struct PSK;

/*
 * Functions to start and stop modem
 * Create returns 0 on memory failure
 * returns 1 if no errors
 */
int psk_create(void);

void psk_destroy(void);

/*
 * Function to produce PCM 16-bit 1-Channel waveform at 8 kHz rate.
 *
 * @param 1 BPSK pilot 1-Channel 32 symbol waveform
 */
void psk_pilot_modulate(int16_t []);

/*
 * Function to produce PCM 16-bit 1-Channel waveform at 8 kHz rate.
 *
 * @param 1 QPSK data 1-Channel 32 symbol waveform
 * @param 2 bits int array of the data bits (32 symbols * 2 bits) = 64 bits
 */
void psk_data_modulate(int16_t [], int []);

/*
 * Function to receive bits from 1-Channel Real 8 kHz sample rate signal
 * 
 * @param 1 an unsigned byte array of the decoded bits
 * @param 2 sync boolean to show sync state
 * @param 3 a real 1-Channel waveform 8 kHz sample rate signal
 */
void psk_receive(uint8_t [], bool *, float []);

float psk_get_SNR(void);
int psk_get_SYNC(void);
int psk_get_NIN(void);
float psk_get_frequency_estimate(void);
float psk_get_fine_frequency_estimate(void);
bool psk_get_clip(void);
void psk_set_clip(bool);

#ifdef	__cplusplus
}
#endif
