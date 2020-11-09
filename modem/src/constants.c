/*---------------------------------------------------------------------------*\

  FILE........: constants.c
  AUTHORS.....: David Rowe & Steve Sampson
  DATE CREATED: October 2020

  Constants used in the QPSK modem

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
#include <complex.h>

/*
 * QPSK Quadrant bit-pair values - Gray Coded
 */
const complex float constellation[] = {
    1.0f + 0.0f * I, //  I
    0.0f + 1.0f * I, //  Q
    0.0f - 1.0f * I, // -Q
    -1.0f + 0.0f * I // -I
};

/*
 * These pilots were randomly generated
 */
const int8_t pilotvalues[] = {
    -1, -1, 1, 1, -1, -1, -1, 1,
    -1, 1, -1, 1, 1, 1, 1, 1,
    1, 1, 1, -1, -1, 1, -1, 1,
    -1, 1, 1, 1, 1, 1, 1, 1, 1
};

/*
 * Created with Octave:
 * hs = gen_rn_coeffs(.35, 1.0/8000.0, 1600, 10, 5);
 */
const float alpha35_root[] = {
    0.00187035f,
    -0.00024537f,
    -0.00220636f,
    -0.00291493f,
    -0.00175708f,
    0.00068764f,
    0.00282391f,
    0.00297883f,
    0.00059170f,
    -0.00311265f,
    -0.00553670f,
    -0.00418297f,
    0.00153693f,
    0.00925400f,
    0.01422443f,
    0.01161151f,
    -0.00045943f,
    -0.01864749f,
    -0.03439334f,
    -0.03667604f,
    -0.01667595f,
    0.02761997f,
    0.08908617f,
    0.15279058f,
    0.20079911f,
    0.21864582f,
    0.20079911f,
    0.15279058f,
    0.08908617f,
    0.02761997f,
    -0.01667595f,
    -0.03667604f,
    -0.03439334f,
    -0.01864749f,
    -0.00045943f,
    0.01161151f,
    0.01422443f,
    0.00925400f,
    0.00153693f,
    -0.00418297f,
    -0.00553670f,
    -0.00311265f,
    0.00059170f,
    0.00297883f,
    0.00282391f,
    0.00068764f,
    -0.00175708f,
    -0.00291493f,
    -0.00220636f,
    -0.00024537f
};