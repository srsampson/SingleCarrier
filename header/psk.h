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
#include <stdbool.h>
#include <stdint.h>

struct PSK;

/*
 * Functions to start and stop modem
 * Create returns -1 on memory failure
 * returns 0 if no errors
 */

int pskCreate(void);     /* create the modem instance */
void pskDestroy(void);   /* close down modem gracefully */

/*
 * Function to return a modulated signal of the 56 bits provided.
 *
 * @param 1 a complex array of the modulated signal
 * @param 2 a int array of the data bits
 * @return int the number of symbols processed
 */
int modulate(complex float [], int []);

/*
 * Function to receive demodulated signals
 * 
 * @param 1 a boolean array of the demodulated bits
 * @param 2 a complex array of the modulated signal
 * @return sync a boolean set to show sync state
 */
int receive(uint8_t [], complex float []);

float getSNR(void);
int getSync(void);
int getNIN(void);
float getCenterFrequency();
float getFrequencyFineEstimate();

#ifdef	__cplusplus
}
#endif
