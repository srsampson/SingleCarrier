/*---------------------------------------------------------------------------*\

  FILE........: constants.c
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
    -1, -1,  1,  1, -1, -1, -1,  1,
    -1,  1, -1,  1,  1,  1,  1,  1,
     1,  1,  1, -1, -1,  1, -1,  1,
    -1,  1,  1,  1,  1,  1,  1, -1
};

/*
 * Created with Octave:
 * hs = gen_rn_coeffs(.50, 1.0/8000.0, 1600, 10, 5);
 */
const float alpha50_root[] = {
    0.002040776f,
    0.001733205f,
    -0.000094696f,
    -0.002190566f,
    -0.002803057f,
    -0.001145122f,
    0.001875377f,
    0.004037490f,
    0.003421695f,
    0.000028693f,
    -0.003768086f,
    -0.004657093f,
    -0.000932888f,
    0.005513738f,
    0.009520251f,
    0.005665029f,
    -0.007427566f,
    -0.024194919f,
    -0.032975574f,
    -0.021014393f,
    0.018508466f,
    0.081140162f,
    0.150832112f,
    0.205501104f,
    0.226202985f,
    0.205501104f,
    0.150832112f,
    0.081140162f,
    0.018508466f,
    -0.021014393f,
    -0.032975574f,
    -0.024194919f,
    -0.007427566f,
    0.005665029f,
    0.009520251f,
    0.005513738f,
    -0.000932888f,
    -0.004657093f,
    -0.003768086f,
    0.000028693f,
    0.003421695f,
    0.004037490f,
    0.001875377f,
    -0.001145122f,
    -0.002803057f,
    -0.002190566f,
    -0.000094696f,
    0.001733205f,
    0.002040776f
};
