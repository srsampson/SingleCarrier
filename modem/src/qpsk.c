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
#include <signal.h>
#include <complex.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/soundcard.h>

#include "qpsk.h"
#include "scramble.h"
#include "fir.h"
#include "fifo.h"
#include "crc.h"

// Prototypes

static float cnormf(complex float);
static float correlate_pilots(complex float [], int);
static float magnitude_pilots(complex float [], int);
static complex float qpsk_mod(int []);
static Rxed qpsk_demod(complex float [], int);
static float find_quadrant_and_distance(int *, complex float);
static int16_t receive_frame(void);
static int tx_frame(int16_t [], complex float [], int);
static void tx_packet(DBlock **, int);
static void tx_symbol(complex float);
static void network_send(void);

// Externals

extern const int8_t pilotvalues[];
extern const complex float constellation[];

extern Queue *pseudo_queue;     // PTY Pseudo-Terminal
extern Queue *packet_queue;     // AX.25 Packet queue

// Globals

MCB mcb;

// Locals

static State state;

static bool dpsk_en;
static bool running;

static complex float tx_filter[NTAPS];
static complex float rx_filter[NTAPS];
static complex float input_frame[(FRAME_SIZE * 2)];
static complex float decimated_frame[562]; // (FRAME_SIZE / CYCLES) * 2
static complex float pilot_table[PILOT_SYMBOLS];
static complex float rx_pilot[PILOT_SYMBOLS];

static int16_t tx_samples[MAX_NR_TX_SAMPLES];

DBlock *inblock[QUEUE_LENGTH];

// Separate phase references for full duplex

static complex float fbb_tx_phase;
static complex float fbb_tx_rect;

static complex float fbb_rx_phase;
static complex float fbb_rx_rect;

static float rx_error;
static float rx_timing;

static int16_t peak;        // peak value PCM samples

// Functions

/*
 * Control-C Interrupt
 */
void intHandler(int d) {
    running = false;
    printf("\nShutting Down...\n");
}

int main(int argc, char **argv) {
    int arg, status;

    /*
     * Create the  Pseudo-Terminal interface
     * pseudo_queue will be created
     */
    if (pseudo_create() == -1) {
        return -1;
    }

    /*
     * Create AX.25 packet interface
     * packet_queue will be created
     */
    if (packet_create() == -1) {
        return -1;
    }

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

    state = HUNT;

    dpsk_en = false;

    /* Modern linux must install sudo apt-get install osspd
     * to get access to /dev/dsp and execute this modem by using
     * the padsp shell:
     *
     * $ padsp ./qpsk
     */
    mcb.fd = open("/dev/dsp", O_RDWR);

    if (mcb.fd != -1) {
        // Bug: in ioctl you must set write parameter first

        arg = 16;
        status = ioctl(mcb.fd, SOUND_PCM_WRITE_BITS, &arg);

        if (status == -1)
            fprintf(stderr, "Can't set write sample size\n");
        else
            fprintf(stderr, "Sound write sample size %d\n", arg);

        arg = 16;
        status = ioctl(mcb.fd, SOUND_PCM_READ_BITS, &arg);

        if (status == -1)
            fprintf(stderr, "Can't set read sample size\n");
        else
            fprintf(stderr, "Sound read sample size %d\n", arg);

        arg = 1;
        status = ioctl(mcb.fd, SOUND_PCM_WRITE_CHANNELS, &arg);

        if (status == -1)
            fprintf(stderr, "Can't set number of write channels\n");
        else
            fprintf(stderr, "Number of write channels %d\n", arg);

        arg = 1;
        status = ioctl(mcb.fd, SOUND_PCM_READ_CHANNELS, &arg);

        if (status == -1)
            fprintf(stderr, "Can't set number of read channels\n");
        else
            fprintf(stderr, "Number of read channels %d\n", arg);

        arg = FS;
        status = ioctl(mcb.fd, SOUND_PCM_WRITE_RATE, &arg);

        if (status == -1)
            fprintf(stderr, "Can't Set write sample rate\n");
        else
            fprintf(stderr, "Write sample rate %d\n", arg);

        arg = FS;
        status = ioctl(mcb.fd, SOUND_PCM_READ_RATE, &arg);

        if (status == -1)
            fprintf(stderr, "Can't Set read sample rate\n");
        else
            fprintf(stderr, "Read sample rate %d\n", arg);

        status = ioctl(mcb.fd, SNDCTL_DSP_SETDUPLEX, 0);

        if (status == -1)
            fprintf(stderr, "Can't Set Full Duplex\n");
        else
            fprintf(stderr, "Full duplex settable\n");

        ioctl(mcb.fd, SNDCTL_DSP_GETCAPS, &arg);

        if ((arg & DSP_CAP_DUPLEX) == 0)
            fprintf(stderr, "Can't Set Full Duplex capability\n");
        else
            fprintf(stderr, "Full duplex capability %d\n", ((arg & DSP_CAP_DUPLEX) == DSP_CAP_DUPLEX) ? 1 : 0);
    } else {
        fprintf(stderr, "Unable to open /dev/dsp %d\n", mcb.fd);
        return -1;
    }

    signal(SIGINT, intHandler); /* Exit gracefully */
    running = true;

    int loop = 0;
    
    /*
     * loop forever
     */
    while (running == true) {
        /*
         * Check for any AX.25 KISS network input data
         */
        pseudo_poll();

        if (state == HUNT) {
            if ((inblock[loop++] = (DBlock *) pseudo_listen()) != NULL) {

                while ((inblock[loop] = pseudo_listen()) != NULL)
                    loop++;

                tx_packet(inblock, loop);

                /*
                 * Send it using modem
                 */
                write(mcb.fd, tx_samples, mcb.sample_count); // int16_t count
            }

            loop = 0;
        }

        /*
         * Check the modem for input data
         */

        peak = receive_frame();
        
        /*
         * Now process any QAM decoded modem input data
         */
        while (packet_queue->state != FIFO_EMPTY) {
            DBlock *dblock = packet_pop();

            /*
             * Send packets to the AX.25 KISS network
             */
            pseudo_write_kiss_data(dblock->data, dblock->length);
        }

        /*
         * Check that the DSP transmit is done
         */
        //ptt_poll();
    }

    packet_destroy();
    pseudo_destroy();

    close(mcb.fd); // Sound descriptor
    close(mcb.pd); // Pseudo TTY descriptor
    //close(mcb.td); // PTT descriptor
    
    return 0;
}

// Functions

int16_t getAudioPeak() {
    return peak;
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
 * Scramble and Gray coded QPSK modulation function
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
    uint8_t data = (bits[1] << 1) | bits[0];
    
    return constellation[scrambleTX(data)];
}

/*
 * Receive function
 *
 * Process a 1600 baud QPSK at 8000 samples/sec.
 *
 * Each voice frame is made up of 33 Pilots and 31 x 8 Data symbols.
 * This is (33 * 5) = 165 + (31 * 5 * 8) = 1240 or 1405 samples per packet
 */
static int16_t receive_frame() {
    Rxed rx_symbol;
    int16_t pcm[FRAME_SIZE * 2];
    complex float fourth = (1.0f / 4.0f);

    /*
     * Input PCM 16-bit from sound card
     */
    if ((read(mcb.fd, pcm, (FRAME_SIZE * 2)) <= 0)) { // read in int16_t (2 * bytes)
        return peak;
    }
    
    for (int j = 0; j < (FRAME_SIZE * 2); j++) {
        if (pcm[j] > peak) {
            peak = pcm[j];
        }
    }

    /*
     * Convert input PCM to complex samples
     * Translate to Baseband at an 8 kHz sample rate
     */
    for (int i = 0; i < FRAME_SIZE; i++) {
        fbb_rx_phase *= fbb_rx_rect;

        complex float val = fbb_rx_phase * ((float) pcm[i] / 16384.0f);

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
        packet_reset();
        resetRXScrambler();

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
        mcb.rx_state = PROCESS;

        /*
         * Now process the QPSK data frame symbols
         */
        for (int i = 0, k = max_index; i < SYMBOLS_PER_BLOCK; i++, k++) {
            rx_symbol = qpsk_demod(decimated_frame, k);
        }
    } else {
        /* zero the accumulated cost value */

        mean = 0.0f;

        /* Process any data left over */

        for (int i = 0, k = max_index; i < SYMBOLS_PER_BLOCK; i++, k++) {
            rx_symbol = qpsk_demod(decimated_frame, k);
            mean += rx_symbol.cost;
        }

        /* Check if reached the end of the frame */

        if (mean > EOF_COST_VALUE) {
            mcb.rx_state = HUNT;
        }
    }
        
    return peak;
}

static float find_quadrant_and_distance(int *quadrant, complex float symbol) {
    float distance;

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
        distance = cnormf(symbol - constellation[i]);

        if (distance < min_value) {
            min_value = distance;
            *quadrant = i;
        }
    }

    return distance;
}

/*
 * Gray coded QPSK demodulation function
 */
Rxed qpsk_demod(complex float in[], int index) {
    Rxed rx_symbol;
    int quadrant;
    
    complex float symbol = in[index];

    /* Include what was transmitted */
    rx_symbol.tx_symb = symbol;

    /* Include cost (distance) for this symbol */
    rx_symbol.cost = find_quadrant_and_distance(&quadrant, symbol);

    /* Include scrambled received (for error compute) */
    rx_symbol.rx_scramble_symb = constellation[quadrant];
    
    /* Return unscrambled data dibit */
    rx_symbol.data = scrambleRX(quadrant);    
    rx_symbol.rx_symb = constellation[rx_symbol.data];
    
    /* Push unscrambled data dibit on packet queue */
    packet_dibit_push(rx_symbol.data);
    
    /* Calculate error */
    rx_symbol.error = (rx_symbol.rx_scramble_symb - symbol) * 0.1f;
    
    return rx_symbol;
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
    state = HUNT;
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
     */
    for (int i = 0; i < (length * CYCLES); i++) {
        frame[i] = (int16_t) (crealf(signal[i]) * 16384.0f); // I at @ .5
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
        tx_samples[mcb.sample_count] = (int16_t) (crealf(signal[i]) * 16384.0f); // I at @ .5
        mcb.sample_count = (mcb.sample_count + 1) % MAX_NR_TX_SAMPLES;
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
    mcb.sample_count = 0;

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

    mcb.sample_count *= 2;  // int16_t count

    write(mcb.fd, tx_samples, mcb.sample_count);
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
