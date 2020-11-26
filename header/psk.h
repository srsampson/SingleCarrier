/*---------------------------------------------------------------------------*\
  
  FILE........: psk.h
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

/* Includes */

#include <complex.h>
#include <stdint.h>

struct PSK;

/*
 * Functions to start and stop modem
 * Create returns -1 on memory failure
 * returns 0 if no errors
 */

int psk_create(void);     /* create the modem instance */
void psk_destroy(void);   /* close down modem gracefully */

/*
 * Function to return an IQ PCM 16-bit 2-Channel waveform at 8 kHz rate.
 *
 * @param waveform complex array of the modulated IQ data and pilots frame
 * @param bits int array of the data only bits (7 * 31 * 2) = 434 bits
 * @return int the number of complex IQ samples (2480 16-bit PCM samples)
 */
int psk_modulate(int16_t [], int []);

/*
 * Function to receive bits from demodulated complex IQ signals
 * 
 * @param 1 a unsigned byte array of the decoded bits
 * @param 2 a complex array of the IQ modulated complex signal
 * @return sync a int set to show sync state
 */
int psk_receive(uint8_t [], complex float []);

float psk_get_SNR(void);
int psk_get_SYNC(void);
int psk_get_NIN(void);
float psk_get_frequency_estimate();
float psk_get_fine_frequency_estimate();
int psk_get_clip(void);
void psk_set_clip(int);

#ifdef	__cplusplus
}
#endif
