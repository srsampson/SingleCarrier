/*
 * equalizer.h
 *
 * Licensed under GNU LGPL V2.1
 * See LICENSE file for information
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "qpsk_internal.h"

float train_eq(complex float [], int, float);
float data_eq(uint8_t *, complex float [], int);

#ifdef __cplusplus
}
#endif
