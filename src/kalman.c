/*
 * kalman.c
 * 
 * Square Root Kalman Filtering for High Speed Data Received
 * over Fading Dispersive Channels.
 * 
 * Transactions on Information Theory, Vol IT-28, No 5
 * 
 * Frank M. Hsu, Member IEEE
 * 
 * Copyright (C) September 1982
 * All Rights Reserved
 */

#include <complex.h>
#include <math.h>

#include "qpsk_internal.h"

// Globals

complex float eq_coeff[EQ_LENGTH];
complex float kalman_gain[EQ_LENGTH];
float kalman_y;

// Locals

static complex float u[EQ_LENGTH][EQ_LENGTH];
static complex float f[EQ_LENGTH];
static complex float h[EQ_LENGTH];

static float d[EQ_LENGTH];
static float a[EQ_LENGTH];

static float E;
static float q;
static float hq;
static float ht;

// Functions

/*
 * Reset variables, to ensure stability
 */
void kalman_reset() {
    for (size_t i = 0; i < EQ_LENGTH; i++) {
        eq_coeff[i] = 0.0f;

        kalman_gain[i] = 0.0f;
        f[i] = 0.0f;
        h[i] = 0.0f;
        d[i] = 1.0f;
 
        for (size_t j = 0; j < EQ_LENGTH; j++) {
            u[i][j] = 0.0f;
        }
    }
}

/*
 * Initialize
 */
void kalman_init() {
    E = 0.1f;
    q = 0.08f;

    kalman_reset();
}

/*
 * Modified Root Kalman gain estimator
 * 
 * x[] is baseband PSK time-domain symbol measurements
 * index is the index into the x[] vector
 * 
 * Gain is small when estimates are stable, and large when
 * estimates are unstable, or we can also say, gain is
 * large when measurements are accurate and small when
 * measurements are inaccurate.
 * 
 * To calculate the kalman gain we need the error in estimate
 * and the error in measurement.
 * 
 *         Eest
 * KG = -----------
 *      Eest + Emea
 */
void kalman_calculate(complex float x[], int index) {
    /*
     * Load Index 0
     */
    f[0] = conjf(x[index]); // 6.2 conjugate of x[0]
    
    /*
     * Load Index 1 through 4
     */
    for (size_t j = 1; j < EQ_LENGTH; j++) {
        f[j] = (u[0][j] * conjf(x[index])) + conj(x[index + j]);
        
        for (size_t i = 1; i < j; i++) {
            f[j] += (u[i][j] * conjf(x[index + i]));
        }
    }

    /*
     * 6.4 g[j] = d[j](k - 1) * f[j]
     */
    for (size_t j = 0; j < EQ_LENGTH; j++) {
        kalman_gain[j] = f[j] * d[j];
    }

    a[0] = E + crealf((kalman_gain[0] * conjf(f[0]))); // 6.5 real part of g[j] times conj f[j]

    for (size_t j = 1; j < EQ_LENGTH; j++) {   // 6.6
        a[j] = a[j - 1] + crealf((kalman_gain[j] * conjf(f[j])));
    }

    hq = 1.0f + q; // 6.7

    ht = a[EQ_LENGTH - 1] * q;      // q = .08

    kalman_y = 1.0f / (a[0] + ht); // 6.19

    d[0] *= hq * (E + ht) * kalman_y; // 6.20

    // 6.10 - 6.16 (Calculate recursively)

    for (size_t j = 1; j < EQ_LENGTH; j++) {
        float B = a[j - 1] + ht; // 6.21

        h[j] = -f[j] * kalman_y; // 6.11

        kalman_y = 1.0f / (a[j] + ht); // 6.22

        d[j] *= hq * B * kalman_y; // 6.13

        for (size_t i = 0; i < j; i++) {
            complex float B1 = u[i][j];
            
            u[i][j] = B1 + (h[j] * conjf(kalman_gain[i])); // 6.15
            kalman_gain[i] += (kalman_gain[j] * conjf(B1)); // 6.16
        }
    }
}

