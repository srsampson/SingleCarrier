/*
 * nrzi.h
 * 
 * Licensed under GNU LGPL V2.1
 * See LICENSE file for information
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>

static int nrzi_state;

void nrzi_init(void);
void nrzi(uint8_t [], uint8_t [], int);

#ifdef __cplusplus
}
#endif
