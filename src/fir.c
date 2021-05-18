/*
 * fir.c
 * 
 * Licensed under GNU LGPL V2.1
 * See LICENSE file for information
 */

#include "fir.h"

// Externals

extern const float alpha35_root[];
extern const float alpha50_root[];

// Locals

static const float *coeff;

/*
 * FIR Filter with specified impulse length used at 8 kHz
 */
void fir(complex float memory[], bool choice, complex float sample[], int length) {
    if (choice == true) {
        coeff = alpha50_root;
    } else {
        coeff = alpha35_root;
    }

    for (size_t j = 0; j < length; j++) {
        for (int i = 0; i < (NTAPS - 1); i++) {
            memory[i] = memory[i + 1];
        }

        memory[(NTAPS - 1)] = sample[j];

        complex float y = 0.0f;

        for (size_t i = 0; i < NTAPS; i++) {
            y += (memory[i] * coeff[i]);
        }

        sample[j] = y * GAIN;
    }
}