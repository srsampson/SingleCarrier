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

#include <complex.h>

typedef struct
{
    complex float rx_symb;
    float cost;
    int data;
    int tx_symb;
} Rxed;

typedef enum
{
    hunt,
    process
} RxState;

void reset_eq(void);
float train_eq(complex float [], int, float);
float data_eq(complex float [], int);

#ifdef __cplusplus
}
#endif

