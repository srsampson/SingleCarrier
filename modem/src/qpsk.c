/*---------------------------------------------------------------------------*\

  FILE........: qpsk.c
  AUTHORS.....: David Rowe & Steve Sampson
  DATE CREATED: October 2020

  A Dynamic Library of functions that implement a QPSK modem

\*---------------------------------------------------------------------------*/
/*
  Copyright (C) 2020 David Rowe

  All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License version 2.1, as
  published by the Free Software Foundation.  This program is
  distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

// Includes

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <complex.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/soundcard.h>

#include "qpsk_internal.h"
#include "scramble.h"
#include "fir.h"
#include "fifo.h"
#include "crc.h"

// Prototypes

static float cnormf(complex float);
static float correlate_pilots(complex float [], int);
static float magnitude_pilots(complex float [], int);
static complex float qpsk_mod(int []);
static void qpsk_demod(complex float, int []);
static int tx_frame(int16_t [], complex float [], int);
static void tx_symbol(complex float);
static void modem_tx(void);
static void network_send(void);

// Externals

extern const int8_t pilotvalues[];
extern const complex float constellation[];

extern Queue *pseudo_queue;
extern Queue *packet_queue;

// Locals

static State state;

static bool dpsk_en;

static complex float tx_filter[NTAPS];
static complex float rx_filter[NTAPS];
static complex float input_frame[(FRAME_SIZE * 2)];
static complex float decimated_frame[562]; // (FRAME_SIZE / CYCLES) * 2
static complex float pilot_table[PILOT_SYMBOLS];
static complex float rx_pilot[PILOT_SYMBOLS];

static int16_t tx_samples[MAX_NR_TX_SAMPLES];

// Separate phase references for full duplex

static complex float fbb_tx_phase;
static complex float fbb_tx_rect;

static complex float fbb_rx_phase;
static complex float fbb_rx_rect;

static float rx_error;
static float rx_timing;

static size_t sample_count; // count of 16-bit PCM samples

static int dsp;

// Functions

int create_qpsk_modem() {
    int arg, status;
    
    /*
     * Create a complex table of pilot values
     * for the correlation algorithm
     */
    for (int i = 0; i < PILOT_SYMBOLS; i++) {
        pilot_table[i] = (float) pilotvalues[i]; // complex -1.0 or 1.0
    }

    /*
     * Initialize center frequency and phase
     */
    fbb_tx_phase = cmplx(0.0f);
    fbb_rx_phase = cmplx(0.0f);

    fbb_tx_rect = cmplx(TAU * CENTER / FS);
    fbb_rx_rect = cmplx(TAU * -CENTER / FS);
    
    rx_timing = FINE_TIMING_OFFSET;
    
    state = hunt;
    
    dpsk_en = false;
    
    dsp = open("/dev/dsp", O_RDWR);
    
    if (dsp != -1) {
        // Bug: in ioctl you must set write parameter first
        
        arg = 16;
        status = ioctl(dsp, SOUND_PCM_WRITE_BITS, &arg);
        
        if (status == -1)
            fprintf(stderr, "Can't set write sample size\n");
        else
            fprintf(stderr, "Sound write sample size %d\n", arg);

        arg = 16;
        status = ioctl(dsp, SOUND_PCM_READ_BITS, &arg);
        
        if (status == -1)
            fprintf(stderr, "Can't set read sample size\n");
        else
            fprintf(stderr, "Sound read sample size %d\n", arg);

        arg = 1;
        status = ioctl(dsp, SOUND_PCM_WRITE_CHANNELS, &arg);
        
        if (status == -1)
            fprintf(stderr, "Can't set number of write channels\n");
        else
            fprintf(stderr, "Number of write channels %d\n", arg);

        arg = 1;
        status = ioctl(dsp, SOUND_PCM_READ_CHANNELS, &arg);
        
        if (status == -1)
            fprintf(stderr, "Can't set number of read channels\n");
        else
            fprintf(stderr, "Number of read channels %d\n", arg);

        arg = FS;
        status = ioctl(dsp, SOUND_PCM_WRITE_RATE, &arg);
        
        if (status == -1)
            fprintf(stderr, "Can't Set write sample rate\n");
        else
            fprintf(stderr, "Write sample rate %d\n", arg);

        arg = FS;
        status = ioctl(dsp, SOUND_PCM_READ_RATE, &arg);
        
        if (status == -1)
            fprintf(stderr, "Can't Set read sample rate\n");
        else
            fprintf(stderr, "Read sample rate %d\n", arg);
        
        status = ioctl(dsp, SNDCTL_DSP_SETDUPLEX, 0);
        
        if (status == -1)
            fprintf(stderr, "Can't Set Full Duplex\n");
        else
            fprintf(stderr, "Full duplex settable\n");
        
        ioctl(dsp, SNDCTL_DSP_GETCAPS, &arg);

        if ((arg & DSP_CAP_DUPLEX) == 0)
            fprintf(stderr, "Can't Set Full Duplex capability\n");
        else
            fprintf(stderr, "Full duplex capability %d\n", ((arg & DSP_CAP_DUPLEX) == DSP_CAP_DUPLEX) ? 1 : 0);
    } else {
        fprintf(stderr, "Unable to open /dev/dsp %d\n", dsp);
        return -1;
    }
    
    return 0;
}

int destroy_qpsk_modem() {
    packet_destroy();
    pseudo_destroy();
    close(dsp);
}

/*
 * For use when sqrt() is not needed
 */
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
 * Return magnitude of symbols (sans sqrt)
 */
static float magnitude_pilots(complex float symbol[], int index) {
    float out = 0.0f;

    for (int i = index; i < (PILOT_SYMBOLS + index); i++) {
        out += cnormf(symbol[i]);
    }
    
    return out;
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
static complex float qpsk_mod(int bits[]) {
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
static void qpsk_demod(complex float symbol, int bits[]) {
    bits[0] = crealf(symbol) < 0.0f; // I < 0 ?
    bits[1] = cimagf(symbol) < 0.0f; // Q < 0 ?
}
/*
 * Receive function
 * 
 * Process a 1600 baud QPSK at 8000 samples/sec.
 * 
 * Each frame is made up of 33 Pilots and 31 x 8 Data symbols.
 * This is (33 * 5) = 165 + (31 * 5 * 8) = 1240 or 1405 samples per packet
 */
void qpsk_rx_frame(int16_t in[], uint8_t bits[]) {
    complex float fourth = (1.0f / 4.0f);

    /*
     * Convert input PCM to complex samples
     * Translate to Baseband at an 8 kHz sample rate
     */
    for (int i = 0; i < FRAME_SIZE; i++) {
        fbb_rx_phase *= fbb_rx_rect;

        complex float val = fbb_rx_phase * ((float) in[i] / 16384.0f);

        input_frame[i] = input_frame[FRAME_SIZE + i];
        input_frame[FRAME_SIZE + i] = val;
    }

    fbb_rx_phase /= cabsf(fbb_rx_phase); // normalize as magnitude can drift

    /*
     * Raised Root Cosine Filter at Baseband
     */
    fir(rx_filter, input_frame, FRAME_SIZE);

    /*
     * Decimate by 5 to the 1600 symbol rate
     */
    for (int i = 0; i < (FRAME_SIZE / CYCLES); i++) {
        int extended = (FRAME_SIZE / CYCLES) + i;  // compute once
        
        decimated_frame[i] = decimated_frame[extended];
        decimated_frame[extended] = input_frame[(i * CYCLES) + (int) roundf(rx_timing)];

        /*
         * Compute the QPSK phase error
         * The BPSK parts will be bogus, but they are short
         */
        float phase_error = cargf(cpowf(decimated_frame[extended], 4.0f) * fourth); // division is slow

        rx_timing = fabsf(phase_error); // only positive
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

    /*
     * Use the magnitude to create a decision point
     */
    mean = magnitude_pilots(decimated_frame, max_index);

    if (max_value > (mean * 30.0f)) {

        /*
         * We probably have a decoded BPSK pilot frame at this point
         */
        for (int i = 0, j = max_index; j < (PILOT_SYMBOLS + max_index); i++, j++) {
            /*
             * Save the pilots for the coherent process
             */
            rx_pilot[i] = decimated_frame[j];
        }
        
        /*
         * Declare process state
         */
        state = process;
        
        /*
         * Now process the QPSK data frame symbols
         */
        


        // qpsk_demod();          // TODO
        
    } else {
        /*
         * Burn the remainder of frame (total loss)
         * 
         * Declare hunt state
         */
        state = hunt;
    }
}


void qpsk_rx_offset(float fshift) {
    fbb_rx_rect *= cmplx(TAU * fshift / FS);
}
/*
 * Dead man switch to state end
 */
void qpsk_rx_end() {
    /*
     * Declare hunt state
     */
    state = hunt;
}

/*
 * Modulate the symbols by first upsampling to 8 kHz sample rate,
 * and translating the spectrum to 1100 Hz, where it is filtered
 * using the root raised cosine coefficients.
 * 
 * These can be either a Pilot or Data Frame
 */
static int tx_frame(int16_t frame[], complex float symbol[], int length) {
    complex float signal[(length * CYCLES)];

    /*
     * Build the 1600 baud packet Frame zero padding
     * for the desired 8 kHz sample rate.
     */
    if (dpsk_en == true) {
        //TODO
        for (int i = 0; i < length; i++) {
            signal[(i * CYCLES)] = symbol[i];

            for (int j = 1; j < CYCLES; j++) {
                signal[(i * CYCLES) + j] = 0.0f;
            }
        }
    } else {
        for (int i = 0; i < length; i++) {
            signal[(i * CYCLES)] = symbol[i];

            for (int j = 1; j < CYCLES; j++) {
                signal[(i * CYCLES) + j] = 0.0f;
            }
        }
    }
    
    /*
     * Root Cosine Filter at Baseband
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
     * Now return the resulting I+Q
     * Note: Summation results in 45 deg phase shift
     */
    for (int i = 0; i < (length * CYCLES); i++) {
        frame[i] = (int16_t) ((crealf(signal[i]) +
                cimagf(signal[i])) * 16384.0f); // I at @ .5
    }
    
    return (length * CYCLES);
}

/*
 * Modulate one symbol
 */
static void tx_symbol(complex float symbol) {
    complex float signal[CYCLES];

    /*
     * Input is 2-bit symbol at 1600 baud
     *
     * Upsample by zero padding for the
     * desired 8000 sample rate.
     */
    signal[0] = symbol;

    for (size_t i = 1; i < CYCLES; i++) {
        signal[i] = 0.0f;
    }

    /*
     * Root Cosine Filter at Baseband
     */
    fir(tx_filter, signal, CYCLES);

    /*
     * Shift Baseband to Center Frequency
     */
    for (size_t i = 0; i < CYCLES; i++) {
        fbb_tx_phase *= fbb_tx_rect;
        signal[i] *= fbb_tx_phase;
    }

    fbb_tx_phase /= cabsf(fbb_tx_phase); // normalize as magnitude can drift

    for (size_t i = 0; i < CYCLES; i++) {
        tx_samples[sample_count] = (int16_t) ((crealf(signal[i]) +
                cimagf(signal[i])) * 16384.0f); // I at @ .5
        sample_count = (sample_count + 1) % MAX_NR_TX_SAMPLES;
    }
}

int qpsk_get_number_of_pilot_bits() {
    return PILOT_SYMBOLS;
}

int qpsk_get_number_of_data_bits() {
    return (DATA_SYMBOLS * 2);
}

/*
 * Return the length in symbols of a frame of PCM symbols at the Center Frequency
 * The modulation data is assumed to be fixed pilot bits
 */
int qpsk_pilot_modulate(int16_t frame[]) {
    return tx_frame(frame, pilot_table, PILOT_SYMBOLS);
}

/*
 * Return the length in symbols of a frame of PCM symbols at the Center Frequency
 * The modulation data is given as unsigned character bits
 * The index is used to avoid pointers, and is a reference into array bits.
 */
int qpsk_data_modulate(int16_t frame[], uint8_t bits[], int index) {
    complex float symbol[DATA_SYMBOLS];
    int dibit[2];

    for (int i = 0, s = index; i < DATA_SYMBOLS; i++, s += 2) {
        dibit[0] = bits[s + 1] & 0x1;
        dibit[1] = bits[s    ] & 0x1;

        symbol[i] = qpsk_mod(dibit);
    }

    return tx_frame(frame, symbol, DATA_SYMBOLS);
}
/*
 * Send raw pilots
 */
static void sendPilots() {
    for (size_t i = 0; i < PILOT_SYMBOLS; i++) {
        tx_symbol(pilot_table[i]);
    }
}
/*
 * Send Four dibits per octet MSB to LSB
 */
int qpsk_raw_modulate(uint8_t octet) {
    int dibit[2];

    for (int i = 6, j = 0; j < 4; i -= 2, j++) {
        dibit[0] = (octet >> (i + 1)) & 0x1;
        dibit[1] = (octet >> i) & 0x1;

        tx_symbol(qpsk_mod(dibit));
    }
}

static void sendEscapedOctet(uint8_t octet) {
    if (octet == FFLAG) {
        qpsk_raw_modulate(FFESC);
        qpsk_raw_modulate(octet ^ 0x20);
    } else if (octet == FFESC) {
        qpsk_raw_modulate(FFESC);
        qpsk_raw_modulate(octet ^ 0x20);
    } else {
        qpsk_raw_modulate(octet);
    }
}

static void sendCRC() {
    uint16_t crc = getCRC();

    sendEscapedOctet((crc >> 8) & 0xFF); // MSB
    sendEscapedOctet(crc & 0xFF);        // LSB
}

/*
 * Preload scrambler LFSR
 */
static void preloadFlush() {
    for (size_t i = 0; i < 8; i++) {
        qpsk_raw_modulate(0x00);
    }
}

/*
 * Construct a transmit packet of count octets
 * from the reference data packet
 */
static void tx_packet(DBlock **packet, int count) {
    sample_count = 0;

    sendPilots();

    resetTXScrambler();
    preloadFlush();

    for (int i = 0; i < count; i++) {
        resetCRC();
        qpsk_raw_modulate(FFLAG);

        for (size_t j = 0; j < packet[i]->length; j++) {
            uint8_t octet = packet[i]->data[j];

            updateCRC(octet);
            sendEscapedOctet(octet);
        }

        sendCRC();
    }

    qpsk_raw_modulate(FFLAG);
    preloadFlush();

    sample_count *= 2;
}

/*
 * After packet is created with tx_packet
 * We send it to the sound card here
 */
static void modem_tx() {
    write(dsp, tx_samples, sample_count); // int16_t count
}

static void network_send() {
    while (packet_queue->state != FIFO_EMPTY) {
        DBlock *packet = packet_pop();

        /*
         * Send packets to the AX.25 KISS network
         */
        pseudo_write_kiss_data(packet->data, packet->length);
    }
}
