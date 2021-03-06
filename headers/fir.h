/*
 * fir.h
 * 
 * Licensed under GNU LGPL V2.1
 * See LICENSE file for information
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "qpsk_internal.h"

#define NTAPS           49
#define GAIN            2.2f

void fir(complex float [], bool, complex float [], int);

#ifdef __cplusplus
}
#endif

