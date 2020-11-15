/*---------------------------------------------------------------------------*\

  FILE........: crc.c
  AUTHORS.....: David Rowe & Steve Sampson
  DATE CREATED: November 2020

  A QPSK modem 16-bit CRC

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

#include <stdint.h>

#include "crc.h"

static uint16_t crcSum;

/*
 * Reset the 16-Bit CRC
 */
void resetCRC() {
    crcSum = 0x0ffff;
}

/*
 * Update the 16-bit CRC
 */
void updateCRC(uint8_t data) {
    uint8_t x = (crcSum >> 8) ^ data;
    
    x ^= (x >> 4);
    
    crcSum = (crcSum << 8) ^ ((uint16_t) (x << 12)) ^
            ((uint16_t) (x << 5)) ^ ((uint16_t) x);
}

uint16_t getCRC() {
    return crcSum;
}