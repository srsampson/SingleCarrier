#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <complex.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h> 

#define NTAPS           49

#define FS              8000.0f
#define RS              1600.0f
#define NS              8
#define TS              (1.0f / RS)
#define CYCLES          (int) (FS / RS)
#define CENTER          1200.0f

#define PILOT_SYMBOLS   33
#define DATA_SYMBOLS    31
    
#define PILOT_SAMPLES   (PILOT_SYMBOLS * CYCLES)
#define DATA_SAMPLES    (DATA_SYMBOLS * CYCLES * NS)
#define FRAME_SIZE      1405

#ifndef M_PI
#define M_PI            3.14159265358979323846f
#endif

#define TAU             (2.0f * M_PI)
#define ROTATE45        (M_PI / 4.0f)

#define cmplx(value) (cosf(value) + sinf(value) * I)
#define cmplxconj(value) (cosf(value) + sinf(value) * -I)

typedef struct
{
    int data;
    int tx_symb;
    float cost;
    complex float rx_symb;
} Rxed;

/* modem state machine states */

typedef enum
{
    hunt,
    process
} State;

/* Prototypes */

complex float qpsk_mod(int []);
void qpsk_demod(complex float, int []);

int bpsk_pilot_modulate(int16_t []);
int qpsk_data_modulate(int16_t [], int [], int);

int tx_frame(int16_t [], complex float [], int);

void rx_frame(int16_t [], int [], FILE *);

#ifdef __cplusplus
}
#endif
