/*
 * costas.c
 *
 * Carrier tracking PLL for QPSK
 *
 * November 2020
 */

// Includes

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <complex.h>
#include <math.h>

#include "qpsk_internal.h"
#include "costas.h"

// Locals

static float d_alpha; // loop gain used for phase adjustment
static float d_beta; // loop gain used for frequency adjustments
static float d_freq; // normalized frequency
static float d_phase; // normalized phase

/*
 * max freq - maximum frequency deviation(radians / sample) loop can handle
 * min freq - minimum frequency deviation(radians / sample) loop can handle
 */

static float d_max_freq;
static float d_min_freq;

// Functions

/*
 * Initialize
 */
void costas_create(float alpha, float beta, float max_freq, float min_freq) {
    d_max_freq = max_freq;
    d_min_freq = min_freq;

    d_alpha = alpha;
    d_beta = beta;
}

/*
 * set the first order gain
 */
void set_alpha(float val) {
    d_alpha = val;
}

/*
 * get the first order gain
 */
float get_alpha() {
    return d_alpha;
}

/*
 * set the second order gain
 */
void set_beta(float val) {
    d_beta = val;
}

/*
 * get the second order gain
 */
float get_beta() {
    return d_beta;
}

/*
 * returns the current NCO frequency in radians/sample
 */
float get_freq() {
    return d_freq;
}

float get_phase() {
    return d_phase;
}

/*
 * sample - complex input sample
 *
 * returns float phase error
 */
static float phase_error(complex float sample) {
    return (
            (crealf(sample) > 0 ? 1.0 : -1.0) * cimagf(sample) -
            (cimagf(sample) > 0 ? 1.0 : -1.0) * crealf(sample)
            );
}

/*
 * items - number of baseband samples
 * iptr - input baseband I and Q
 * optr - output I and Q
 * foptr - normalized frequency I and Q
 */
void costas(int items, complex float *iptr, complex float *optr, complex float *foptr) {
    for (int i = 0; i < items; i++) {
        optr[i] = iptr[i] * cmplx(-d_phase);

        float error = phase_error(optr[i]);

        if (error > 1.0f)
            error = 1.0f;
        else if (error < -1.0f)
            error = -1.0f;

        d_freq += (d_beta * error);
        d_phase += (d_freq + (d_alpha * error));

        while (d_phase > TAU)
            d_phase -= TAU;

        while (d_phase < -TAU)
            d_phase += TAU;

        if (d_freq > d_max_freq)
            d_freq = d_max_freq;
        else if (d_freq < d_min_freq)
            d_freq = d_min_freq;

        foptr[i] = d_freq;
    }
}

