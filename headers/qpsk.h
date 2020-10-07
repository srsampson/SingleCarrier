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

#define OSC_TABLE_SIZE  32
#define NZEROS          41

#define FS              8000.0f
#define RS              1600.0f
#define TS              (1.0f / RS)
#define CYCLES          (int) (FS / RS)
#define CENTER          1200.0f
#define SCALE           4096.0f

#define PILOT_SYMBOLS   33
#define DATA_SYMBOLS    31
    
#define PILOT_SAMPLES   165
#define DATA_SAMPLES    1240
#define FRAME_SIZE      (PILOT_SAMPLES + DATA_SAMPLES)

// @ (33 * 5) + ((31 * 5) * 8)
#define TX_SAMPLES_SIZE 1500
#define RX_SAMPLES_SIZE 1500

#ifndef M_PI
#define M_PI        3.14159265358979323846f
#endif

#define TAU         (2.0f * M_PI)
#define ROT45       (M_PI / 4.0f)

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

complex float qpsk_mod(int *);
void qpsk_demod(complex float, int *);
void bpsk_modulate(int [], int);
void qpsk_modulate(int [], int);
void rx_frame_init(void);
void receive_frame(int16_t [], int *, FILE *);

#ifdef __cplusplus
}
#endif

