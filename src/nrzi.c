/*
 * nrzi.c
 * 
 * Licensed under GNU LGPL V2.1
 * See LICENSE file for information
 */

#include "nrzi.h"

static int nrzi_state;

void nrzi_init() {
    nrzi_state = 0;
}

/*
 * NRZI Non-Return-to-Zero Inverted
 */
void nrzi(uint8_t out[], uint8_t in[], int length) {

    int state = nrzi_state;

    for (size_t i = 0; i < length; i++) {
        if (in[i] == 1) {       // Toggle on Mark
            // change state
            state = !state;
        }
        
        out[i] = state;
    }

    nrzi_state = state;
}
