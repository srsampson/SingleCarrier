/*
 * scramble.h
 * 
 * Licensed under GNU LGPL V2.1
 * See LICENSE file for information
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "qpsk_internal.h"

#define SEED 0x4A80
#define BITS 2

/* Constants  */

typedef enum {
    tx,
    rx,
    both
} SRegister;

/* Prototypes */

void scramble_init(SRegister);
int scramble(uint8_t *, SRegister);

#ifdef __cplusplus
}
#endif
