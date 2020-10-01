#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "qpsk.h"

extern float rrccoeff[RRCLEN];
extern complex float osc_table[OSC_TABLE_SIZE];
extern float pilot_table[PILOT_SAMPLES];

static complex float rx_filter[RRCLEN];
static complex float rx_frame[RX_SAMPLES_SIZE * 2];
static complex float proc_frame[RX_SAMPLES_SIZE];

static int rx_osc_offset;

static float cnormf(complex float val) {
    float realf = crealf(val);
    float imagf = cimagf(val);

    return realf * realf + imagf * imagf;
}

static complex float rx_fir(complex float sample[]) {
    complex float y = 0.0f;

    for (int i = 0; i < RRCLEN; i++) {
        y += (sample[i] * rrccoeff[i]);
    }

    return y;
}

static float correlate_pilots(complex float sample[], int index) {
    float out = 0.0f;

    for (int i = 0, j = index; i < PILOT_SAMPLES; i++, j++) {
        out += (crealf(sample[j]) * pilot_table[i]);
    }

    return sqrt(out * out);
}

void receive_frame(int16_t in[], int *bits, FILE *fout) {
    float mean, val;
    int max_index = 0;

    for (int i = 0; i < RX_SAMPLES_SIZE; i++) {
        val = (float) in[i] / 32767.0f;

        rx_frame[i] = rx_frame[RX_SAMPLES_SIZE + i];
        rx_frame[RX_SAMPLES_SIZE + i] = osc_table[rx_osc_offset] * val;
        rx_osc_offset = (rx_osc_offset + 1) % OSC_TABLE_SIZE;
    }

    // Downsample the 5 cycles at 8 kHz Sample rate

    for (int i = 0; i < (RX_SAMPLES_SIZE / 5); i++) {
        proc_frame[i] = proc_frame[(RX_SAMPLES_SIZE / 5) + i];
        proc_frame[(RX_SAMPLES_SIZE / 5) + i] = rx_fir(&rx_frame[i * 5]);

        int16_t samp = (int16_t) (crealf(proc_frame[i]) * 32767.0f);
        fwrite(&samp, sizeof (int16_t), 1, fout);
    }

    /* Hunting for the pilot preamble sequence */

    float temp_value = 0.0f;
    float max_value = 0.0f;

    for (int i = 0; i < (RX_SAMPLES_SIZE / 5); i++) {
        temp_value = correlate_pilots(proc_frame, i);

        if (temp_value > max_value) {
            max_value = temp_value;
            max_index = i;
        }
    }

    // unknown if correct yet
    
    printf("Max Index = %d\n", max_index);
}

