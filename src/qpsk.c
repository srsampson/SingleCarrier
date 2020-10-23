#define TEST_OUT
#define TEST2
/*
 * qpsk.c
 *
 * Testing program for qpsk modem algorithms
 * October 2020
 */

// Includes

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <complex.h>
#include <string.h>
#include <time.h>

#include "qpsk.h"
#include "filter_coef.h"

// Prototypes

static float cnormf(complex float);
static void freq_shift(complex float [], complex float [], int, int, float, complex float);
static void fir(complex float [], complex float [], int);
static float correlate_pilots(complex float [], int);
static float magnitude_pilots(complex float [], int);

// Globals

static FILE *fin;
static FILE *fout;

static complex float tx_filter[NTAPS];
static complex float rx_filter[NTAPS];
static complex float input_frame[(FRAME_SIZE * 2)];
static complex float decimated_frame[564]; /* file scope warning in GNU using var */
static complex float pilot_table[PILOT_SYMBOLS];

// Two phase for full duplex

static complex float fbb_tx_phase;
static complex float fbb_tx_rect;

static complex float fbb_rx_phase;
static complex float fbb_rx_rect;

/*
 * QPSK Quadrant bit-pair values - Gray Coded
 * Non-static so they can be used by other modules
 */
const complex float constellation[] = {
    1.0f + 0.0f * I, //  I
    0.0f + 1.0f * I, //  Q
    0.0f - 1.0f * I, // -Q
    -1.0f + 0.0f * I // -I
};

/*
 * These pilots were randomly generated
 */
const int8_t pilotvalues[] = {
    -1, -1, 1, 1, -1, -1, -1, 1,
    -1, 1, -1, 1, 1, 1, 1, 1,
    1, 1, 1, -1, -1, 1, -1, 1,
    -1, 1, 1, 1, 1, 1, 1, 1, -1
};

// Functions

static float cnormf(complex float val) {
    float realf = crealf(val);
    float imagf = cimagf(val);

    return realf * realf + imagf * imagf;
}

/*
 * FIR Filter with specified impulse length used at 8 kHz
 */
static void fir(complex float memory[], complex float sample[], int length) {
    for (int j = 0; j < length; j++) {
        for (int i = 0; i < (NTAPS - 1); i++) {
            memory[i] = memory[i + 1];
        }

        memory[(NTAPS - 1)] = sample[j];

        complex float y = 0.0f;

        for (int i = 0; i < NTAPS; i++) {
            y += (memory[i] * alpha31_root[i]);
        }

        sample[j] = y;
    }
}

/*
 * Sliding Window
 */
static float correlate_pilots(complex float symbol[], int index) {
    complex float out = 0.0f;

    for (int i = 0, j = index; i < PILOT_SYMBOLS; i++, j++) {
        out += (pilot_table[i] * symbol[j]);
    }

    return cnormf(out);
}

static float magnitude_pilots(complex float symbol[], int index) {
    complex float out = 0.0f;

    for (int i = index; i < (PILOT_SYMBOLS + index); i++) {
        out += (symbol[i] * symbol[i]);
    }

    return (crealf(out) + cimagf(out));
}

/*
 * Useful for operator offset of receiver fine tuning
 */
static void freq_shift(complex float out[], complex float in[], int index,
        int length, float fshift, complex float phase_rect) {

    complex float foffset_rect = cmplx(TAU * fshift / FS);

    /*
     * Use a copy of the receive data to leave it alone
     * for other algorithms (Probably not needed).
     */
    complex float *copy = (complex float *) calloc(sizeof (complex float), length);

    for (int i = index, j = 0; i < length; i++, j++) {
        copy[j] = in[index];
    }

    for (int i = 0; i < length; i++) {
        phase_rect *= foffset_rect;
        out[i] = copy[i] * phase_rect;
    }

    free(copy);

    phase_rect /= cabsf(phase_rect); // normalize as magnitude can drift
}

/*
 * Receive function
 * 
 * Basically we receive a 1600 baud QPSK at 8000 samples/sec.
 * 
 * The signal has an I and a Q channel, so we receive PCM as stereo.
 * 
 * Each frame is made up of 33 Pilots and 31 x 8 Data symbols.
 * This is (33 * 5) = 165 + (31 * 5 * 8) = 1240 or 1405 samples per packet
 * 
 * Since there is the I and the Q, this means we will receive 2810 samples
 * per packet, or frame size * 2.
 */
void rx_frame(int16_t in[], int bits[], FILE *fout) {
    int16_t pcm[FRAME_SIZE * 2];

    /*
     * Convert input stereo PCM to I and Q complex samples
     * Translate to baseband at 8 kHz sample rate
     */
    for (int i = 0, j = 0; j < FRAME_SIZE; i += 2, j++) {
        fbb_rx_phase *= fbb_rx_rect;

        float valI = ((float) in[i] / 16384.0f); // convert back to +/- .5
        float valQ = ((float) in[i + 1] / 16384.0f);
        complex float temp = (valI + valQ * I);

        input_frame[j] = input_frame[FRAME_SIZE + j];
        input_frame[FRAME_SIZE + j] = temp * fbb_rx_phase;
    }

    fbb_rx_phase /= cabsf(fbb_rx_phase); // normalize as magnitude can drift

    /*
     * Raised Root Cosine Filter
     */
    fir(rx_filter, input_frame, FRAME_SIZE);

#ifdef TEST_OUT
    /* Display baseband */

    for (int i = 0, j = 0; j < FRAME_SIZE; i += 2, j++) {

        /*
         * Output the frame in stereo at 8000 samples/sec
         */
        pcm[i] = (int16_t) (crealf(input_frame[j]) * 16384.0f);
        pcm[i + 1] = (int16_t) (cimagf(input_frame[j]) * 16384.0f);
    }

    fwrite(pcm, sizeof (int16_t), (FRAME_SIZE * 2), fout);
#endif

    /*
     * Decimate by 5 to the 1600 symbol rate
     */
    for (int i = 0; i < (FRAME_SIZE / CYCLES); i++) {
        decimated_frame[i] = decimated_frame[(FRAME_SIZE / CYCLES) + i];
        decimated_frame[(FRAME_SIZE / CYCLES) + i] = input_frame[(i * CYCLES) + FINE_TIMING_OFFSET];
#ifdef TEST_SCATTER
        fprintf(stderr, "%f %f\n", crealf(decimated_frame[(FRAME_SIZE / CYCLES) + i]), cimagf(decimated_frame[(FRAME_SIZE / CYCLES) + i]));
#endif 
    }

    int dibit[2];

#ifdef TEST1
    /*
     * Just list the demodulated values
     */
    for (int i = 0; i < (FRAME_SIZE / CYCLES); i++) {
        qpsk_demod(decimated_frame[i], dibit);

        printf("%d%d ", dibit[1], dibit[0]);
    }

    printf("\n\n");
#endif

#ifdef TEST2
    /*
     * List the correlation match
     */

    /* Hunting for the pilot preamble sequence */

    float temp_value = 0.0f;
    float max_value = 0.0f;
    float mean = 0.0f;
    int max_index = 0;

    for (int i = 0; i < ((FRAME_SIZE / CYCLES) / 2); i++) {
        temp_value = correlate_pilots(decimated_frame, i);

        if (temp_value > max_value) {
            max_value = temp_value;
            max_index = i;
        }
    }

    mean = magnitude_pilots(decimated_frame, max_index);

    printf("%d %.2f %.2f\n", max_index, max_value, mean);
    
    if (max_value > (mean * 20.0f)) {
        for (int i = max_index; i < (PILOT_SYMBOLS + max_index); i++) {
            complex float symbol = decimated_frame[i];
            
            qpsk_demod(symbol, dibit);

            printf("%d%d ", dibit[1], dibit[0]);
        }
    }

    printf("\n\n");
#endif
}

/*
 * Gray coded QPSK modulation function
 * 
 *      Q
 *      |
 * -I---+---I
 *      |
 *     -Q
 * 
 * The symbols are not rotated on transmit
 */
complex float qpsk_mod(int bits[]) {
    return constellation[(bits[1] << 1) | bits[0]];
}

/*
 * Gray coded QPSK demodulation function
 *
 *  Q 01   00 I
 *     \ | /
 *      \|/
 *   ----+----
 *      /|\
 *     / | \
 * -I 11   10 -Q
 * 
 * By rotating received symbol 45 degrees left the
 * bits are easier to decode as they are in a specific
 * rectangular quadrant.
 * 
 * Each bit pair differs from the next by only one bit.
 */
void qpsk_demod(complex float symbol, int bits[]) {
    complex float rotate = symbol * cmplx(ROTATE45);

    bits[0] = crealf(rotate) < 0.0f; // I < 0 ?
    bits[1] = cimagf(rotate) < 0.0f; // Q < 0 ?
}

/*
 * Modulate the symbols by first upsampling to 8 kHz sample rate,
 * and translating the spectrum to 1200 Hz, where it is filtered
 * using the root raised cosine coefficients.
 */
int tx_frame(int16_t samples[], complex float symbol[], int length) {
    complex float signal[(length * CYCLES)];

    /*
     * Build the 1600 baud packet Frame zero padding
     * for 8 kHz sample rate.
     */
    for (int i = 0; i < length; i++) {
        signal[(i * CYCLES)] = symbol[i];

        for (int j = 1; j < CYCLES; j++) {
            signal[(i * CYCLES) + j] = 0.0f;
        }
    }

    /*
     * Root Cosine Filter
     */
    fir(tx_filter, signal, (length * CYCLES));

    /*
     * Shift Baseband to 1200 Hz Center Frequency
     */
    for (int i = 0; i < (length * CYCLES); i++) {
        fbb_tx_phase *= fbb_tx_rect;
        signal[i] *= fbb_tx_phase;
    }

    fbb_tx_phase /= cabsf(fbb_tx_phase); // normalize as magnitude can drift

    /*
     * Now return the resulting I+Q samples
     */
    for (int i = 0, j = 0; j < (length * CYCLES); i += 2, j++) {
        complex float temp = signal[j];

        samples[i] = (int16_t) (crealf(temp) * 16384.0f); // I at @ .5
        samples[i + 1] = (int16_t) (cimagf(temp) * 16384.0f); // Q at @ .5
    }

    return (length * CYCLES) * 2;
}

int bpsk_pilot_modulate(int16_t samples[]) {
    return tx_frame(samples, pilot_table, PILOT_SYMBOLS);
}

int qpsk_data_modulate(int16_t samples[], int tx_bits[], int length) {
    complex float symbol[length];
    int dibit[2];

    for (int i = 0, s = 0; i < length; i++, s += 2) {
        dibit[0] = tx_bits[s + 1] & 0x1;
        dibit[1] = tx_bits[s ] & 0x1;

        symbol[i] = qpsk_mod(dibit);
    }

    return tx_frame(samples, symbol, length);
}

// Main Program

int main(int argc, char** argv) {
    int bits[6400];
    int16_t frame[FRAME_SIZE * 2];
    int length;

    srand(time(0));

    /*
     * Create a complex float table of pilot values
     * for correlation algorithm
     */
    for (int i = 0; i < PILOT_SYMBOLS; i++) {
        pilot_table[i] = (float) pilotvalues[i]; // complex -1.0 or 1.0
    }

    /*
     * create the BPSK/QPSK pilot time-domain waveform
     */
    fout = fopen(TX_FILENAME, "wb");

    /*
     * This simulates the transmitted packets
     */

    fbb_tx_phase = cmplx(0.0f);
    fbb_tx_rect = cmplx(TAU * CENTER / FS);

    for (int k = 0; k < 500; k++) {
        // 33 BPSK pilots
        length = bpsk_pilot_modulate(frame);

        fwrite(frame, sizeof (int16_t), length, fout);

        /*
         * NS data frames between each pilot frame
         */
        for (int j = 0; j < NS; j++) {
            // 31 QPSK
            for (int i = 0; i < (DATA_SYMBOLS * 2); i += 2) {
                bits[i] = rand() % 2;
                bits[i + 1] = rand() % 2;
            }

            length = qpsk_data_modulate(frame, bits, DATA_SYMBOLS);

            fwrite(frame, sizeof (int16_t), length, fout);
        }
    }

    fclose(fout);

    /*
     * Now try to process what was transmitted
     */
    fin = fopen(TX_FILENAME, "rb");
    fout = fopen(RX_FILENAME, "wb");

    fbb_rx_phase = cmplx(0.0f);
    fbb_rx_rect = cmplx(TAU * -CENTER / FS);

    while (1) {
        /*
         * Read in the 2810 I+Q samples
         */
        size_t count = fread(frame, sizeof (int16_t), (FRAME_SIZE * 2), fin);

        if (count != (FRAME_SIZE * 2))
            break;

        rx_frame(frame, bits, fout);
    }

    fclose(fin);
    fclose(fout);

    return (EXIT_SUCCESS);
}
