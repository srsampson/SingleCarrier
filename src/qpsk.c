#define TEST2
#define TEST_OUT_1
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

// Prototypes

static void freq_shift(complex float [], complex float [], int, int, float, complex float []);
static complex float fir(complex float [], complex float);
static float correlate_pilots(float [], int);
static float magnitude_pilots(float [], int);

// Defines
#define TX_FILENAME "/tmp/spectrum-filtered.raw"
#define RX_FILENAME "/tmp/spectrum.raw"

// Globals

static FILE *fin;
static FILE *fout;

static complex float tx_filter[NTAPS];
static complex float rx_filter[NTAPS];
static complex float input_frame[(FRAME_SIZE * 2)];
static complex float pilot_table[PILOT_SYMBOLS];

static float process_frame[(FRAME_SIZE / CYCLES) * 2];

static int sync_position;

// Two phase for full duplex
// Scripted to avoid pointers

static complex float fbb_tx_phase[1];
static complex float fbb_tx_rect[1];

static complex float fbb_rx_phase[1];
static complex float fbb_rx_rect[1];
/*
 * QPSK Quadrant bit-pair values - Gray Coded
 * Non-static so they can be used by other modules
 */
const complex float constellation[] = {
    1.0f + 0.0f * I,    //  I
    0.0f + 1.0f * I,    //  Q
    0.0f - 1.0f * I,    // -Q
    -1.0f + 0.0f * I    // -I
};

/*
 * These pilots are compatible with Octave version
 * Non-static so they can be used by other modules
 */
const int8_t pilotvalues[] = {
    -1, -1, 1, 1, -1, -1, -1, 1,
    -1, 1, -1, 1, 1, 1, 1, 1,
    1, 1, 1, -1, -1, 1, -1, 1,
    -1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, -1, 1, 1, 1, 1,
    1, -1, -1, -1, -1, -1, -1, 1,
    -1, 1, -1, 1, -1, -1, 1, -1,
    1, 1, 1, 1, -1, 1, -1, 1
};

/*
 * Created with:
 * hs = gen_rn_coeffs(.31, 1.0/8000.0, 1600, 10, 5);
 */
static const float alpha31_root[] = {
  -0.00140721, -0.00258347, -0.00211782, -0.00010823,  0.00224934,  0.00326078,
   0.00179380, -0.00179940, -0.00552621, -0.00663829, -0.00324267,  0.00418549,  0.01233564,
   0.01618214,  0.01144857, -0.00262892, -0.02165701, -0.03666715, -0.03709688, -0.01518676,
   0.03002121,  0.09095027,  0.15306638,  0.19943502,  0.21659606,  0.19943502,  0.15306638,
   0.09095027,  0.03002121, -0.01518676, -0.03709688, -0.03666715, -0.02165701, -0.00262892,
   0.01144857,  0.01618214,  0.01233564,  0.00418549, -0.00324267, -0.00663829, -0.00552621,
  -0.00179940,  0.00179380,  0.00326078,  0.00224934, -0.00010823, -0.00211782, -0.00258347,
  -0.00140721
};

// Functions

/*
 * FIR Filter with specified impulse length
 */
static complex float fir(complex float memory[], complex float sample) {
    for (int i = (NTAPS - 1); i > 0; i--) {
        memory[i] = memory[i - 1];
    }

    memory[0] = sample;

    complex float y = 0.0f;

    for (int i = 0; i < NTAPS; i++) {
        y += (memory[i] * alpha31_root[i]);
    }

    return y;
}

/*
 * Sliding Window
 */
static float correlate_pilots(float symbol[], int index) {
    float out = 0.0f;

    for (int i = 0, j = index; i < PILOT_SYMBOLS; i++, j++) {
        out += (crealf(pilot_table[i]) * symbol[j]);
    }

    return (out * out);
}

static float magnitude_pilots(float symbol[], int index) {
    float out = 0.0f;
    
    for (int i = index; i < (PILOT_SYMBOLS + index); i++) {
        out += symbol[i];
    }

    return (out * out);
}

/*
 * Useful for operator receiver fine tuning
 */
static void freq_shift(complex float out[], complex float in[], int index,
        int length, float fshift, complex float phase_rect[]) {
    
    complex float foffset_rect = cmplx(TAU * fshift / FS);
    
    /*
     * Use a copy of the receive data to leave it alone
     * for other algorithms (Probably not needed).
     */
    complex float *copy = (complex float *) calloc(sizeof(complex float), length);

    for (int i = index, j = 0; i < length; i++, j++) {
        copy[j] = in[index];
    }

    for (int i = 0; i < length; i++) {
	phase_rect[0] *= foffset_rect;
	out[i] = copy[i] * phase_rect[0];
    }

    free(copy);

    phase_rect[0] /= cabsf(phase_rect[0]);    // normalize as magnitude can drift
}

/*
 * Receive function
 */
void rx_frame(int16_t in[], int bits[], FILE *fout) {
    int16_t pcm[FRAME_SIZE * 2];

    /*
     * Downshifts the 1200 Hz center frequency to baseband
     * by swapping the I and Q channels
     */

    for (int i = 0, j = 0; i < FRAME_SIZE; i += 2, j++) {
        fbb_rx_phase[0] *= fbb_rx_rect[0];

        float valI = ((float) in[i] / 16384.0f); // convert to +/- 1.0
        float valQ = ((float) in[i + 1] / 16384.0f);
        complex float temp = (valQ + valI * I);   // Swap I and Q
        
        input_frame[j] = input_frame[FRAME_SIZE + j];
        input_frame[FRAME_SIZE + j] = temp * fbb_rx_phase[0]; //fir(rx_filter, temp * fbb_rx_phase[0]);
    }

    fbb_rx_phase[0] /= cabsf(fbb_rx_phase[0]);    // normalize as magnitude can drift

    /*
     * Decimate by 5 to the 1600 symbol rate
     * 
     * Swap the I channel back and forget about the Q (Which is zero now)
     *
     * There will be two frames of symbols in order to slide the window
     * past the first half during processing.
     */
    for (int i = 0; i < (FRAME_SIZE / CYCLES); i++) {
        process_frame[i] = process_frame[(FRAME_SIZE / CYCLES) + i];
        process_frame[(FRAME_SIZE / CYCLES) + i] = cimagf(input_frame[(i * CYCLES)]);
    }

#ifdef TEST_OUT_1
    /*
     * Output Stereo at 8000 samples/sec
     */
    for (int i = 0, j = 0; j < FRAME_SIZE; i += 2, j++) {
        // testing (swap the I and Q)
        pcm[i] = (int16_t) (cimagf(input_frame[j]) * 16384.0f);
        pcm[i+1] = (int16_t) (crealf(input_frame[j]) * 16384.0f);
    }

    fwrite(pcm, sizeof (int16_t), FRAME_SIZE, fout);
#endif

#ifdef TEST_OUT_2
    /*
     * Output Mono at 1600 samples/sec
     */

    for (int i = 0; i < (FRAME_SIZE / CYCLES); i++) {
        // testing
        pcm[i] = (int16_t) (process_frame[i] * 16384.0f);
    }  
    
    fwrite(pcm, sizeof (int16_t), (FRAME_SIZE / CYCLES), fout);
#endif
  
    int dibit[2];

#ifdef TEST1
    /*
     * Just list the demodulated values
     */
    for (int i = 0; i < (FRAME_SIZE / CYCLES); i++) {
        qpsk_demod(process_frame[i], dibit);
        
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

    for (int i = 0; i < (FRAME_SIZE / CYCLES); i++) {
        temp_value = correlate_pilots(process_frame, i);

        if (temp_value > max_value) {
            max_value = temp_value;
            max_index = i;
        }
    }
    
    sync_position = max_index;
    mean = magnitude_pilots(process_frame, sync_position);
    
    printf("%d %.2f\n", max_index, mean);
    
    for (int i = sync_position; i < (PILOT_SYMBOLS + sync_position); i++) {
        float symbol = process_frame[i];
        qpsk_demod(symbol, dibit);
        
        printf("%d%d ", dibit[1], dibit[0]);
    }
    
    printf("\n\n");
#endif
    
    fflush(stdout);
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

    bits[0] = crealf(rotate) < 0.0f;    // I < 0 ?
    bits[1] = cimagf(rotate) < 0.0f;    // Q < 0 ?
}

/*
 * Encode the symbol while upsampling to 8 kHz sample rate
 * using the root raised cosine filter, and a center frequency
 * of 1200 Hz to center the audio in the 300 to 3 kHz limits
 */
int tx_frame(int16_t samples[], complex float symbol[], int length) {
    complex float signal[(length * CYCLES)];
    
    for (int k = 0; k < length; k++) {
        for (int j = 0; j < CYCLES; j++) {
            fbb_tx_phase[0] *= fbb_tx_rect[0];
            
            signal[(k * CYCLES) + j] = symbol[k] * fbb_tx_phase[0]; //fir(tx_filter, symbol[k] * fbb_tx_phase[0]);
        }
    }

    fbb_tx_phase[0] /= cabsf(fbb_tx_phase[0]); // normalize as magnitude can drift

    for (int i = 0, j = 0; i < ((length * CYCLES) * 2); i += 2, j++) {
        complex float temp = signal[j];

        samples[i] = (int16_t) (crealf(temp) * 16384.0f); // I
        samples[i+1] = (int16_t) (cimagf(temp) * 16384.0f); // Q
    }
    
    return length * CYCLES * 2;
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
    
    fbb_tx_phase[0] = cmplx(0.0f);
    fbb_tx_rect[0] = cmplx(TAU * CENTER / FS);
    
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

    fflush(fout);
    fclose(fout);

    /*
     * Now try to process what was transmitted
     */
    fin = fopen(TX_FILENAME, "rb");
    fout = fopen(RX_FILENAME, "wb");

    fbb_rx_phase[0] = cmplx(0.0f);
    fbb_rx_rect[0] = cmplx(TAU * CENTER / FS);

    while (1) {
        size_t count = fread(frame, sizeof (int16_t), FRAME_SIZE, fin);

        if (count != FRAME_SIZE)
            break;

        rx_frame(frame, bits, fout);
    }

    fclose(fin);
    fflush(fout);
    fclose(fout);

    return (EXIT_SUCCESS);
}
