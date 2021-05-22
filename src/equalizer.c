/*
 * equalizer.c
 * 
 * Licensed under GNU LGPL V2.1
 * See LICENSE file for information
 */

#include "qpsk_internal.h"

#include "kalman.h"
#include "equalizer.h"
#include "scramble.h"

// Externals

extern complex float eq_coeff[];
extern complex float kalman_gain[];
extern float kalman_y;

// Functions

/*
 * Update coefficients using gain vector and error
 */
static void update_eq(complex float in[], int index, complex float error) {
    /*
     * Calculate the new gain
     */
    kalman_calculate(in, index);

    /*
     * Create filter coefficients using
     * the kalman gain and error (uncertainty)
     */
    error *= kalman_y;
    
    for (size_t i = 0; i < EQ_LENGTH; i++) {
        eq_coeff[i] += (error * conjf(kalman_gain[i]));
    }
}

/*
 * Returns the real value for BPSK demodulation.
 */
float train_eq(complex float in[], int index, float ref) {
    complex float val = 0.0f;

    for (size_t i = 0, j = index; i < EQ_LENGTH; i++, j++) {
        val += (in[j] * eq_coeff[i]);
    }

    /* Calculate error */
    complex float error = conjf(ref - val);

    update_eq(in, index, error);

    return crealf(error);
}

/*
 * Returns the bits, and distance for the PSK symbol
 * and updates the equalization filter
 */
float data_eq(uint8_t *bits, complex float in[], int index) {
    uint8_t dibit[2]; // IQ bit values

    complex float symbol = 0.0f;
    
    for (size_t i = 0, j = index; i < EQ_LENGTH; i++, j++) {
        symbol += (in[j] * conjf(eq_coeff[i]));
    }

    qpsk_demod(dibit, symbol);

    float i = (dibit[1] == 1) ? -1.0f : 1.0f; // I Odd
    float q = (dibit[0] == 1) ? -1.0f : 1.0f; // Q Even

    complex float constellation = i + q * I;

    /* Calculate error */
    complex float error = (constellation - symbol) * 0.1f;

    update_eq(in, index, error);

    *bits = (dibit[1] << 1) | dibit[0]; // IQ

    scramble(bits, rx);

    return crealf(error);
}
