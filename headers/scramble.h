/*
 * scramble.h
 * 
 * Licensed under GNU LGPL V2.1
 * See LICENSE file for information
 */

#pragma once

#ifdef	__cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>

#define SEED 0x4A80
#define BITS 2

/* Prototypes */

void resetTXScrambler(void);
void resetRXScrambler(void);
uint8_t scrambleTX(uint8_t);
uint8_t scrambleRX(uint8_t);

#ifdef	__cplusplus
}
#endif
