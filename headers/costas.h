/*
 * costas.h
 *
 * November 2020
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <complex.h>

void costas_create(float alpha, float beta, float max_freq, float min_freq);

/*
 * set the first order gain
 */
void set_alpha(float alpha);

/*
 * get the first order gain
 */
float get_alpha(void);

/*
 * set the second order gain
 */
void set_beta(float);

/*
 * get the second order gain
 */
float get_beta(void);

/*
 * returns the current NCO frequency in radians/sample
 */
float get_freq(void);

float get_phase(void);

/*
 * Main function
 */
void costas(int, complex float *, complex float *, complex float *);

#ifdef __cplusplus
}
#endif

