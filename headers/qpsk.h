#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <complex.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h> 

#define OSC_TABLE_SIZE  32
#define TX_SAMPLES_SIZE (33 * (31 * 8))
#define RRCLEN          39

#define FS              8000.0f
#define RS              1600.0f
#define TS              (1.0f / RS)
#define CYCLES          (int) (FS / RS)
#define CENTER          1200.0f
#define SCALE           8192.0f

#ifndef M_PI
#define M_PI        3.14159265358979323846f
#endif

#define TAU         (2.0f * M_PI)
#define ROT45       (M_PI / 4.0f)

#define cmplx(value) (cosf(value) + sinf(value) * I)
#define cmplxconj(value) (cosf(value) + sinf(value) * -I)

/* modem state machine states */
typedef enum {
    hunt,
    process
} State;

/* Prototypes */

#ifdef __cplusplus
}
#endif

