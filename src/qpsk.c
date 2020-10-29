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

#include "qpsk_internal.h"
#include "fir.h"

// Prototypes

static float cnormf(complex float);
static int find_quadrant(complex float);
static void freq_shift(complex float [], complex float [], int, int, float, complex float);
static float correlate_pilots(complex float [], int);
static float magnitude_pilots(complex float [], int);
static void receive_end(void);

// Externals

extern const int8_t pilotvalues[];
extern const complex float constellation[];
extern const float alpha31_root[];

// Globals

static FILE *fin;
static FILE *fout;

static State state;

static complex float tx_filter[NTAPS];
static complex float rx_filter[NTAPS];
static complex float input_frame[(FRAME_SIZE * 2)];
static complex float decimated_frame[562]; // (FRAME_SIZE / CYCLES) * 2
static complex float pilot_table[PILOT_SYMBOLS];
static complex float rx_pilot[PILOT_SYMBOLS];

// Two phase for full duplex

static complex float fbb_tx_phase;
static complex float fbb_tx_rect;

static complex float fbb_rx_phase;
static complex float fbb_rx_rect;

#ifdef DEBUG
    int pilot_frames_detected = 0;
#endif

// Functions

static float cnormf(complex float val) {
    float realf = crealf(val);
    float imagf = cimagf(val);

    return realf * realf + imagf * imagf;
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

/*
 * Save time, skip sqrt()
 */
static float magnitude_pilots(complex float symbol[], int index) {
    float out = 0.0f;

    for (int i = index; i < (PILOT_SYMBOLS + index); i++) {
        out += cnormf(symbol[i]);
    }
    
    return out;
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

static int find_quadrant(complex float symbol) {
    float quadrant;

    /*
     * The smallest distance between constellation
     * and the symbol, is our gray coded quadrant.
     * 
     *      1
     *      |
     *  3---+---0
     *      |
     *      2
     */

    float min_value = 200.0f; // some large value

    for (int i = 0; i < 4; i++) {
        float dist = cnormf(symbol - constellation[i]);

        if (dist < min_value) {
            min_value = dist;
            quadrant = i;
        }
    }

    return quadrant;
}

/*
 * Receive function
 * 
 * Basically we receive a 1600 baud QPSK at 8000 samples/sec.
 * 
 * Each frame is made up of 33 Pilots and 31 x 8 Data symbols.
 * This is (33 * 5) = 165 + (31 * 5 * 8) = 1240 or 1405 samples per packet
 */
void rx_frame(int16_t in[], int bits[], FILE *fout) {
    complex float val;

    /*
     * Convert input PCM to complex samples
     * Translate to baseband at an 8 kHz sample rate
     */
    for (int i = 0; i < FRAME_SIZE; i++) {
        fbb_rx_phase *= fbb_rx_rect;
        
        val = ((float) in[i] / 16384.0f) + 0.0f * I;
        
        input_frame[i] = input_frame[FRAME_SIZE + i];
        input_frame[FRAME_SIZE + i] = val * fbb_rx_phase;
    }

    fbb_rx_phase /= cabsf(fbb_rx_phase); // normalize as magnitude can drift

    /*
     * Raised Root Cosine Filter
     */
    fir(rx_filter, input_frame, FRAME_SIZE);

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

    if (max_value > (mean * 30.0f)) {
#ifdef DEBUG
        pilot_frames_detected++;
        printf("Frames: %d MaxIdx: %d MaxVal: %.2f Mean: %.2f\n",
                pilot_frames_detected, max_index, max_value, mean);
#endif
        for (int i = 0, j = max_index; j < (PILOT_SYMBOLS + max_index); i++, j++) {
            /*
             * Save the pilots for the coherent process
             */
            rx_pilot[i] = decimated_frame[j];
        }
        
        /*
         * Now process data symbols TODO
         */
        
        state = process;
    } else {
        /*
         * Burn remainder of frame
         */
        state = hunt;
    }
}

/*
 * Dead man switch to state end
 */
static void receive_end() {
    state = hunt;
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
 * By transmitting I+Q at transmitter, the signal has a
 * 45 degree shift, which is just what we need to find
 * the quadrant the symbol is in.
 * 
 * Each bit pair differs from the next by only one bit.
 */
void qpsk_demod(complex float symbol, int bits[]) {
    bits[0] = crealf(symbol) < 0.0f; // I < 0 ?
    bits[1] = cimagf(symbol) < 0.0f; // Q < 0 ?
}

/*
 * Modulate the symbols by first upsampling to 8 kHz sample rate,
 * and translating the spectrum to 1100 Hz, where it is filtered
 * using the root raised cosine coefficients.
 */
int tx_frame(int16_t samples[], complex float symbol[], int length) {
    complex float signal[(length * CYCLES)];

    /*
     * Build the 1600 baud packet Frame zero padding
     * for the desired 8 kHz sample rate.
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
     * Shift Baseband to Center Frequency
     */
    for (int i = 0; i < (length * CYCLES); i++) {
        fbb_tx_phase *= fbb_tx_rect;
        signal[i] *= fbb_tx_phase;
    }

    fbb_tx_phase /= cabsf(fbb_tx_phase); // normalize as magnitude can drift

    /*
     * Now return the resulting real samples
     * (imaginary part discarded)
     */
    for (int i = 0; i < (length * CYCLES); i++) {
        samples[i] = (int16_t) ((crealf(signal[i]) + cimagf(signal[i])) * 16384.0f); // @ .5
    }

    return (length * CYCLES);
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
    int16_t frame[FRAME_SIZE];
    int length;

    srand(time(0));

    /*
     * Create a complex table of pilot values
     * for the correlation algorithm
     */
    for (int i = 0; i < PILOT_SYMBOLS; i++) {
        pilot_table[i] = (float) pilotvalues[i]; // complex -1.0 or 1.0
    }

    /*
     * create the BPSK pilot and QPSK data waveform.
     * This simulates the transmitted packets.
     */
    fout = fopen(TX_FILENAME, "wb");

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
         * Read in the frame samples
         */
        size_t count = fread(frame, sizeof (int16_t), FRAME_SIZE, fin);

        if (count != FRAME_SIZE)
            break;

        rx_frame(frame, bits, fout);
    }

    fclose(fin);
    fclose(fout);

    return (EXIT_SUCCESS);
}
