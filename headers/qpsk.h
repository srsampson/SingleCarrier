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

#define NZEROS          41

#define FS              8000.0f
#define RS              1600.0f
#define NS              8
#define TS              (1.0f / RS)
#define CYCLES          (int) (FS / RS)
#define CENTER          1200.0f
#define QUADRANTS       4
    
// Make it double size for grins (size = 40 for 1600 baud)
#define OSC_TABLE_SIZE  (int)(((FS / RS) * QUADRANTS) * 2)

// C doesn't like variable array sizes in globals
#define OSC_FIXED_SIZE  (5 * 4 * 2)

#define PILOT_SYMBOLS   33
#define DATA_SYMBOLS    31
    
#define PILOT_SAMPLES   (PILOT_SYMBOLS * CYCLES)
#define DATA_SAMPLES    (DATA_SYMBOLS * CYCLES * NS)
#define FRAME_SIZE      (PILOT_SAMPLES + DATA_SAMPLES)

// @ (33 * 5) + ((31 * 5) * 8)
#define TX_SAMPLES_SIZE 1500
#define RX_SAMPLES_SIZE 1500

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

void flush_fir_memory(complex float *memory);

void bpsk_pilot_modulate(void);
void qpsk_data_modulate(int [], int);

void tx_frame(complex float [], int);
void tx_frame_reset(void);
void tx_flush(void);

void rx_frame(int16_t [], int []);

#ifdef __cplusplus
}
#endif
