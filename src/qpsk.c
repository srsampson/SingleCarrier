#define TEST1
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

static float cnormf(complex float);
static complex float fir(complex float *, complex float [], int);
static float correlate_pilots(complex float [], int);
static float magnitude_pilots(complex float [], int);

// Defines
#define TX_FILENAME "/tmp/spectrum-filtered.raw"
#define RX_FILENAME "/tmp/spectrum.raw"

// Globals

static FILE *fin;
static FILE *fout;

static complex float tx_filter[NTAPS];
static complex float rx_filter[NTAPS];
static complex float input_frame[RX_SAMPLES_SIZE * 2];  // Input Samples * 2
static complex float process_frame[RX_SAMPLES_SIZE];    // Input Symbols * 2

static complex float pilot_table[PILOT_SYMBOLS];

static int16_t tx_samples[TX_SAMPLES_SIZE];

static int tx_osc_offset;
static int rx_osc_offset;
static int tx_sample_offset;
static int sync_position;

static complex float fbb_phase[1];
static complex float fbb_rect[1];

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
 * hs = gen_rn_coeffs(.35, 0.000125, 1600, 8, 5);
 */
static const float rrccoeff[] = {
    0.00265568,
    0.00287612,
    0.00063112,
    -0.00295307,
    -0.00535752,
    -0.00409702,
    0.00147671,
    0.00908714,
    0.01405700,
    0.01154864,
    -0.00037972,
    -0.01847733,
    -0.03424136,
    -0.03663832,
    -0.01677543,
    0.02744741,
    0.08895061,
    0.15277814,
    0.20091748,
    //
    0.21881953,
    //
    0.20091748,
    0.15277814,
    0.08895061,
    0.02744741,
    -0.01677543,
    -0.03663832,
    -0.03424136,
    -0.01847733,
    -0.00037972,
    0.01154864,
    0.01405700,
    0.00908714,
    0.00147671,
    -0.00409702,
    -0.00535752,
    -0.00295307,
    0.00063112,
    0.00287612,
    0.00265568
};

// Functions

static float cnormf(complex float val) {
    float realf = crealf(val);
    float imagf = cimagf(val);

    return realf * realf + imagf * imagf;
}

/*
 * FIR Filter with specified impulse length
 */
static complex float fir(complex float *memory, complex float sample[], int index) {
    for (int i = (NTAPS - 1); i > 0; i--) {
        memory[i] = memory[i - 1];
    }

    memory[0] = sample[index];

    complex float y = 0.0f;

    for (int i = 0; i < NTAPS; i++) {
        y += (memory[i] * rrccoeff[i]);
    }

    return y;
}

/*
 * Sliding Window
 */
static float correlate_pilots(complex float symbol[], int index) {
    complex float out = 0.0f;

    for (int i = 0, j = index; i < PILOT_SYMBOLS; i++, j++) {
        out += (symbol[j] * pilot_table[i]);
    }

    return cnormf(out);
}

static float magnitude_pilots(complex float symbol[], int index) {
    complex float out = 0.0f;
    
    for (int i = index; i < (PILOT_SYMBOLS + index); i++) {
        out += symbol[i];
    }

    return cnormf(out);
}

/*
 * Useful for operator receiver fine tuning
 */
void freq_shift(complex float out[], complex float in[], int index,
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
    complex float val[1];
    int16_t pcm[(RX_SAMPLES_SIZE / CYCLES)];

    /*
     * FIR filter and downshift to baseband
     */
    for (int i = 0; i < RX_SAMPLES_SIZE; i++) {
        fbb_phase[0] *= fbb_rect[0];

        val[0] = (float) in[i] / 16384.0f;   // convert to +/- 1.0
        
        input_frame[i] = input_frame[RX_SAMPLES_SIZE + i];
        //input_frame[RX_SAMPLES_SIZE + i] = fir(rx_filter, val, 0) * fbb_phase[0];
        input_frame[RX_SAMPLES_SIZE + i] = val[0] * fbb_phase[0];
    }

    /*
     * Downsample the 5 cycles at 8 kHz Sample rate to symbols (1600 Baud)
     *
     * There will be two frames of symbols in order to slide the window
     * past the first half during processing.
     */
    for (int i = 0; i < (RX_SAMPLES_SIZE / CYCLES); i++) {
        process_frame[i] = process_frame[(RX_SAMPLES_SIZE / CYCLES) + i];
        process_frame[(RX_SAMPLES_SIZE / CYCLES) + i] = input_frame[(i * CYCLES)];
        
        // testing
        pcm[i] = (int16_t) (crealf(process_frame[i]) * 16384.0f);
    }
    
    fwrite(pcm, sizeof (int16_t), (RX_SAMPLES_SIZE / CYCLES), fout);
    
    fbb_phase[0] /= cabsf(fbb_phase[0]);    // normalize as magnitude can drift

    int dibit[2];

#ifdef TEST1
    
    for (int i = 0; i < (RX_SAMPLES_SIZE / CYCLES); i++) {
        qpsk_demod(process_frame[i], dibit);
        
        printf("%d%d ", dibit[1], dibit[0]);
    }
    
    printf("\n\n");
#endif
    
#ifdef TEST2
    /* Hunting for the pilot preamble sequence */

    float temp_value = 0.0f;
    float max_value = 0.0f;
    float mean = 0.0f;
    int max_index = 0;

    for (int i = 0; i < (RX_SAMPLES_SIZE / CYCLES); i++) {
        temp_value = correlate_pilots(process_frame, i);

        if (temp_value > max_value) {
            max_value = temp_value;
            max_index = i;
        }
    }
    
    mean = magnitude_pilots(process_frame, max_index);
    sync_position = max_index;
    
    printf("%d %.2f\n", max_index, mean);
    
    for (int i = sync_position; i < (PILOT_SYMBOLS + sync_position); i++) {
        complex float symbol = process_frame[i];
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
void tx_frame(complex float symbol[], int length) {
    complex float signal;

    /*
     * Here we go from the 1600 symbol rate by
     * upsampling each symbol to the 8 kHz sample rate
     */
    for (int k = 0; k < length; k++) {
        for (int j = 0; j < CYCLES; j++) {
            fbb_phase[0] *= fbb_rect[0];
            
            signal = /*fir(tx_filter, symbol, k)*/symbol[k] * fbb_phase[0];
            
            tx_samples[tx_sample_offset] = (int16_t) (crealf(signal) * 16384.0f);
            tx_sample_offset = (tx_sample_offset + 1) % TX_SAMPLES_SIZE;
        }
    }

    fbb_phase[0] /= cabsf(fbb_phase[0]);    // normalize as magnitude can drift
}

/*
 * Zero out the selected FIR delay memory
 */
void flush_fir_memory(complex float *memory) {
    for (int i = 0; i < NTAPS; i++) {
        memory[i] = 0.0f;
    }
}

void tx_frame_reset() {
    tx_sample_offset = 0;
    tx_osc_offset = 0;
}

/*
 * Transmit CYCLES (5) null symbols
 */
void tx_flush() {
    complex float symbol[CYCLES];

    for (int i = 0; i < CYCLES; i++) {
        symbol[i] = 0.0f;
    }

    tx_frame(symbol, CYCLES);
}

void bpsk_pilot_modulate() {
    tx_frame(pilot_table, PILOT_SYMBOLS);
}

void qpsk_data_modulate(int tx_bits[], int length) {
    complex float symbol[length];
    int dibit[2];

    for (int i = 0, s = 0; i < length; i++, s += 2) {
        dibit[0] = tx_bits[s + 1] & 0x1;
        dibit[1] = tx_bits[s ] & 0x1;

        symbol[i] = qpsk_mod(dibit);
    }

    tx_frame(symbol, length);
}

// Main Program

int main(int argc, char** argv) {
    int bits[6400];
    int16_t frame[RX_SAMPLES_SIZE];

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
     * Initialize the tx FIR filter memory for
     * this transmission of packets
     */
    flush_fir_memory(tx_filter);
    
    /*
     * This simulates the transmitted packets
     */
    
    fbb_phase[0] = cmplx(0.0f);
    fbb_rect[0] = cmplx(TAU * CENTER / FS);

    for (int k = 0; k < 500; k++) {
        tx_frame_reset();
        tx_flush();
    
        // 33 BPSK pilots
        bpsk_pilot_modulate();
        tx_flush();

        /*
         * NS data frames between each pilot frame
         */
        for (int j = 0; j < NS; j++) {
            // 31 QPSK
            for (int i = 0; i < (DATA_SYMBOLS * 2); i += 2) {
                bits[i] = 0;//rand() % 2;
                bits[i + 1] = 0;//rand() % 2;
            }
            
            qpsk_data_modulate(bits, DATA_SYMBOLS);
        }

        fwrite(tx_samples, sizeof (int16_t), tx_sample_offset, fout);
    }

    fflush(fout);
    fclose(fout);

    /*
     * Now try to process what was transmitted
     */
    fin = fopen(TX_FILENAME, "rb");
    fout = fopen(RX_FILENAME, "wb");
    
    fbb_phase[0] = cmplx(0.0f);
    fbb_rect[0] = cmplx(0.0f);

    while (1) {
        size_t count = fread(frame, sizeof (int16_t), RX_SAMPLES_SIZE, fin);

        if (count != RX_SAMPLES_SIZE)
            break;

        rx_frame(frame, bits, fout);
    }

    fclose(fin);
    fflush(fout);
    fclose(fout);

    return (EXIT_SUCCESS);
}
