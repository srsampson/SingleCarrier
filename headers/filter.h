/*
  Copyright (C) 2018 James C. Ahlstrom

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

#include <complex.h>

struct quisk_cfFilter {         // Structure to hold the static data for FIR filters
    complex float *cpxCoefs;    // complex filter coefficients
    complex float *cSamples;    // storage for old samples
    complex float *ptcSamp;     // next available position in cSamples
    float *dCoefs;              // real filter coefficients
    int nTaps;                  // dimension of dSamples, cSamples, dCoefs
};

extern void quisk_filt_cfInit(struct quisk_cfFilter *, float *, int);
extern void quisk_filt_destroy(struct quisk_cfFilter *);
extern void quisk_cfTune(struct quisk_cfFilter *, float);
extern void quisk_ccfFilter(complex float *, complex float *, int, struct quisk_cfFilter *);

extern float alpha31_root[49];
