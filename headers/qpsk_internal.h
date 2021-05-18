/*
 * qpsk_internal.h
 *
 * Licensed under GNU LGPL V2.1
 * See LICENSE file for information
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <complex.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h> 
    
#define FINE_TIMING_OFFSET 3

#define TX_FILENAME "/tmp/spectrum-filtered.raw"
#define RX_FILENAME "/tmp/databits.txt"
    
#define EOF_COST_VALUE  5.0f
    
#define EQ_LENGTH       5

#define FS              8000.0f
#define RS              1600.0f
#define TS              (1.0f / RS)
#define CYCLES          (int) (FS / RS)
#define CENTER          1100.0f

#define NS              8
#define DATA_SYMBOLS    31
#define FRAME_SYMBOLS   (DATA_SYMBOLS * NS)
#define DATA_SAMPLES    (DATA_SYMBOLS * CYCLES * NS)

// Frame Size = DATA_SYMBOLS * NS * CYCLES
#define FRAME_SIZE      1240

// (DATA_SYMBOLS * 2 bits * NS)
#define BITS_PER_FRAME  496

#define PREAMBLE_LENGTH 128

#ifndef M_PI
#define M_PI            3.14159265358979323846f
#endif

#define TAU             (2.0f * M_PI)
#define ROT45           (M_PI / 4.0f)

/*
 * This method is much faster than using cexp()
 * float_value - must be a float
 */
#define cmplx(float_value) (cosf(float_value) + sinf(float_value) * I)
#define cmplxconj(float_value) (cosf(float_value) + sinf(float_value) * -I)

// Enum
    
typedef enum
{
    hunt,
    process
} RXState;

// Prototypes

float cnormf(complex float);

complex float qpsk_mod(uint8_t [], int);
void qpsk_demod(uint8_t bits[], complex float symbol);
int qpsk_rx_frame(int16_t [], uint8_t []);
int qpsk_tx_frame(int16_t [], complex float [], int);

#ifdef __cplusplus
}
#endif
