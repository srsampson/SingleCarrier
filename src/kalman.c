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

#include <math.h>

#include "v32.h"
#include "kalman.h"

// Locals

static complex float u[EQ_LENGTH][EQ_LENGTH];

static complex float f[EQ_LENGTH];
static complex float g[EQ_LENGTH];
static complex float h[EQ_LENGTH];

static float d[EQ_LENGTH];
static float a[EQ_LENGTH];

static float E;
static float q;
static float hq;
static float y;
static float ht;

// Functions

/*
 * Reset variables, to ensure stability
 */
void kalman_reset() {
    for (size_t i = 0; i < EQ_LENGTH; i++) {
        f[i] = 0.0f;
        g[i] = 0.0f;
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
void init_kalman() {
    E = 0.1f;
    q = 0.08f;

    kalman_reset();
}

/*
 * Modified Root Kalman gain estimator
 */
static void kalman_calculate(complex float x[], int index) {
    f[0] = conjf(x[index + 0]); // 6.2 conjugate of x[0]
    
    /*
     *  6.3 Add conjugate of x[j] to product of u[][] * conjugate x[]
     */
    for (size_t j = 1; j < EQ_LENGTH; j++) {
        f[j] = (u[0][j] * conjf(x[index])) + conjf(x[index + j]);

        for (size_t i = 1; i < j; i++) {
            f[j] += (u[i][j] * conjf(x[index + i]));
        }
    }

    /*
     * 6.4 g[j] = d[j](k - 1) * f[j]
     */
    for (size_t j = 0; j < EQ_LENGTH; j++) {
        g[j] = f[j] * d[j];
    }

    a[0] = E + crealf((g[0] * conjf(f[0]))); // 6.5 real part of g[j] times conj f[j]

    for (size_t j = 1; j < EQ_LENGTH; j++) {   // 6.6
        a[j] = a[j - 1] + crealf((g[j] * conjf(f[j])));
    }

    hq = 1.0f + q; // 6.7

    ht = a[EQ_LENGTH - 1] * q;

    y = 1.0f / (a[0] + ht); // 6.19

    d[0] = d[0] * hq * (E + ht) * y; // 6.20

    // 6.10 - 6.16 (Calculate recursively)

    for (size_t j = 1; j < EQ_LENGTH; j++) {
        float B = a[j - 1] + ht; // 6.21

        h[j] = -f[j] * y; // 6.11

        y = 1.0f / (a[j] + ht); // 6.22

        d[j] = d[j] * hq * B * y; // 6.13

        for (size_t i = 0; i < j; i++) {
            complex float B0 = u[i][j];
            
            u[i][j] = B0 + (h[j] * conjf(g[i])); // 6.15
            g[i] += (g[j] * conjf(B0)); // 6.16
        }
    }
}

/*
 * Update coefficients using Kalman gain vector and error
 */
void kalman_update(complex float coffs[], complex float data[], int index, complex float error) {
    /*
     * Calculate the new Kalman gain vector
     */
    kalman_calculate(data, index);

    /*
     * Update the filter coefficients using the gain vector
     * and the error.
     */
    error *= y;

    for (size_t i = 0; i < EQ_LENGTH; i++) {
        coffs[i] += (error * conjf(g[i]));
    }
}
