/*---------------------------------------------------------------------------*\

  FILE........: crc.h
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

#include <stdint.h>

/*
 * Initialize the CRC with 16-bit 0xffff value
 */
void resetCRC(void);

/*
 * Use the 8-bit byte to update the 16-bit CRC value
 */
void updateCRC(uint8_t);

/*
 * Return a copy of the current 16-bit CRC
 */
uint16_t getCRC(void);

#ifdef __cplusplus
}
#endif