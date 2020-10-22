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
#include "filter.h"

// Prototypes

static float cnormf(complex float);
static void freq_shift(complex float [], complex float [], int, int, float, complex float);
static float correlate_pilots(complex float [], int);
static float magnitude_pilots(complex float [], int);

// Defines
#define TX_FILENAME "/tmp/spectrum-filtered.raw"
#define RX_FILENAME "/tmp/spectrum.raw"

// Globals

static FILE *fin;
static FILE *fout;

static complex float input_frame[(FRAME_SIZE * 2)];
static complex float decimated_frame[564]; /* file scope warning in GNU using var */
static complex float pilot_table[PILOT_SYMBOLS];

static int sync_position;

// Two phase for full duplex

static complex float fbb_tx_phase;
static complex float fbb_tx_rect;

static complex float fbb_rx_phase;
static complex float fbb_rx_rect;

static struct quisk_cfFilter *cbpf;

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
    -1, 1, 1, 1, 1, 1, 1, 1, -1
};

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

    return cabsf(out);
}

static float magnitude_pilots(complex float symbol[], int index) {
    complex float out = 0.0f;
    
    for (int i = index; i < (PILOT_SYMBOLS + index); i++) {
        out += symbol[i];
    }

    return cabsf(out);
}

/*
 * Useful for operator receiver fine tuning
 */
static void freq_shift(complex float out[], complex float in[], int index,
        int length, float fshift, complex float phase_rect) {
    
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
	phase_rect *= foffset_rect;
	out[i] = copy[i] * phase_rect;
    }

    free(copy);

    phase_rect /= cabsf(phase_rect);    // normalize as magnitude can drift
}

/*
 * Receive function
 * 
 * Basically we receive a 1600 baud QPSK signal where each symbol
 * is made up of 5 cycles which results in (1600 * 5) 8000 samples/sec.
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
    complex float bpfilt[FRAME_SIZE];
    int16_t pcm[FRAME_SIZE * 2];

    /*
     * Convert to I and Q complex samples
     * 
     * Shift the 1200 Hz Center Frequency to Baseband
     */
    for (int i = 0, j = 0; j < FRAME_SIZE; i += 2, j++) {
        fbb_rx_phase *= fbb_rx_rect;

#ifdef TEST_OUT_1
        /*
         * Output the original frame in Stereo at 8000 samples/sec
         */
        pcm[i] = in[i];
        pcm[i + 1] = in[i + 1];
#endif

        float valI = ((float) in[i] / 16384.0f); // convert back to +/- .5
        float valQ = ((float) in[i + 1] / 16384.0f);
        complex float temp = (valI + valQ * I);

        input_frame[j] = input_frame[FRAME_SIZE + j];
        input_frame[FRAME_SIZE + j] = (temp * fbb_rx_phase);
    }

    fbb_rx_phase /= cabsf(fbb_rx_phase);    // normalize as magnitude can drift

    /*
     * Complex Root Cosine Filter
     */
    quisk_ccfFilter(input_frame, bpfilt, FRAME_SIZE, cbpf);

#ifdef TEST_OUT_1
    /* Unshifted 1200 Hz audio */
    
    fwrite(pcm, sizeof (int16_t), (FRAME_SIZE * 2), fout);
#endif
#ifdef TEST_OUT_2
    /* Shifted to baseband */
    
    for (int i = 0, j = 0; j < FRAME_SIZE; i += 2, j++) {
        
        /*
         * Output the original frame in stereo at 8000 samples/sec
         */
        pcm[i] = (int16_t)(crealf(bpfilt[j]) * 16384.0f);
        pcm[i + 1] = (int16_t)(cimagf(bpfilt[j]) * 16384.0f);
    }
    
    fwrite(pcm, sizeof (int16_t), (FRAME_SIZE * 2), fout);
#endif
        
    /*
     * Decimate by 5 to the 1600 symbol rate
     */
    for (int i = 0; i < (FRAME_SIZE / CYCLES); i++) {
        decimated_frame[i] = decimated_frame[(FRAME_SIZE / CYCLES) + i];
        decimated_frame[(FRAME_SIZE / CYCLES) + i] = bpfilt[(i * CYCLES)];
    }

#ifdef TEST_OUT_3
    /*
     * Output only real 1600 samples/sec
     */

    for (int i = 0; i < (FRAME_SIZE / CYCLES); i++) {
        // testing
        pcm[i] = (int16_t) (crealf(decimated_frame[i]) * 16384.0f);
    }  
    
    fwrite(pcm, sizeof (int16_t), (FRAME_SIZE / CYCLES), fout);
#endif
  
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
    
    sync_position = max_index;
    mean = magnitude_pilots(decimated_frame, sync_position);
    
    printf("%d %.2f\n", sync_position, mean);
    
    for (int i = sync_position; i < (PILOT_SYMBOLS + sync_position); i++) {
        complex float symbol = decimated_frame[i];
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
    complex float bpfilt[(length * CYCLES)];

    /*
     * Build the 1600 baud packet Frame
     * into 1405 samples
     */
    for (int i = 0; i < length; i++) {
        for (int j = 0; j < CYCLES; j++) {
            signal[(i * CYCLES) + j] = symbol[i];
        }
    }

    /*
     * Complex Root Cosine Filter
     */

    quisk_ccfFilter(signal, bpfilt, (length * CYCLES), cbpf);
    memmove(signal, bpfilt, (length * CYCLES) * sizeof (complex float));

    /*
     * Shift Baseband to 1200 Hz Center Frequency
     */
    for (int i = 0; i < (length * CYCLES); i++) {
        fbb_tx_phase *= fbb_tx_rect;
        signal[i] *= fbb_tx_phase;
    }
    
    fbb_tx_phase /= cabsf(fbb_tx_phase); // normalize as magnitude can drift

    /*
     * Now return the I+Q samples
     */
    for (int i = 0, j = 0; j < (length * CYCLES); i += 2, j++) {
        complex float temp = signal[j];

        samples[i] = (int16_t) (crealf(temp) * 16384.0f);   // I at @ .5
        samples[i+1] = (int16_t) (cimagf(temp) * 16384.0f); // Q at @ .5
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

    cbpf = malloc(sizeof(struct quisk_cfFilter));

    /* cbpf = complex coefficients, center frequency */

    quisk_filt_cfInit(cbpf, alpha31_root, sizeof (alpha31_root) / sizeof (float));
    quisk_cfTune(cbpf, CENTER / FS);

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

    fflush(fout);
    fclose(fout);

    /*
     * Now try to process what was transmitted
     */
    fin = fopen(TX_FILENAME, "rb");
    fout = fopen(RX_FILENAME, "wb");

    fbb_rx_phase = cmplx(0.0f);
    fbb_rx_rect = cmplx(TAU * CENTER / FS);

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
    fflush(fout);
    fclose(fout);

    quisk_filt_destroy(cbpf);

    return (EXIT_SUCCESS);
}
