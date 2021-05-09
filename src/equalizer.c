/*
 * equalizer.c
 *
 * Licensed under GNU LGPL V2.1
 * See LICENSE file for information
 */

#include <math.h>

#include "kalman.h"
#include "equalizer.h"

// Globals

static complex float c_eq[EQ_LENGTH];

// Functions

void reset_eq() {
    for (int i = 0; i < EQ_LENGTH; i++) {
        c_eq[i] = 0.0f;
    }

    kalman_reset();
}

/*
 * Returns the real value for BPSK demodulation.
 */
float train_eq(complex float in[], int index, float ref) {
    complex float val = 0.0f;

    for (int i = 0, j = index; i < EQ_LENGTH; i++, j++) {
        val += (in[j] * c_eq[i]);
    }

    /* Calculate error */
    complex float error = conjf(ref - val);

    kalman_update(c_eq, in, index, error);

    return crealf(error);
}

/*
 *  Returns the path metric for the symbol
 */
float data_eq(complex float in[], int index) {
    complex float symbol = 0.0f;

    for (int i = 0, j = index; i < EQ_LENGTH; i++, j++) {
        symbol += (in[j] * c_eq[i]);
    }

    symbol = conjf(symbol);

    /* Symbol decode */
    Rxed rb = rx_symbol(symbol);

    /* Calculate error */
    complex float error = (rb.rx_symb - symbol) * 0.1f;

    kalman_update(c_eq, in, index, error);

    return rb.cost;
}
