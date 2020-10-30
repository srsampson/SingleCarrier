/*---------------------------------------------------------------------------*\

  FILE........: qpsk.c
  AUTHORS.....: David Rowe & Steve Sampson
  DATE CREATED: October 2020

  A Library of functions that implement a QPSK modem

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

#define DEBUG

#define OPTPARSE_IMPLEMENTATION
#define OPTPARSE_API static
#include "optparse.h"

// Includes

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <complex.h>
#include <string.h>

#include "qpsk_internal.h"
#include "fir.h"

#define IS_DIR_SEPARATOR(c) ((c) == '/')

// Prototypes

static complex float qpsk_mod(int []);
static int tx_frame(int16_t [], complex float [], int, bool);
static int bpsk_pilot_modulate(int16_t []);
static int qpsk_data_modulate(int16_t [], uint8_t [], int);

// Externals

extern const int8_t pilotvalues[];
extern const complex float constellation[];

// Globals

static bool dpsk_en = false;
static bool use_text = false;

static bool input_specified;
static bool output_specified;
static bool test_frames;

static int verbose;
static int Nsec = 0;

static complex float tx_filter[NTAPS];
static complex float pilot_table[PILOT_SYMBOLS];
static int16_t frame[FRAME_SIZE];
static uint8_t bits[BITS_PER_FRAME];

static complex float fbb_tx_phase;
static complex float fbb_tx_rect;

static const char *progname;

void opt_help() {
    fprintf(stderr, "\nusage: %s [options]\n\n", progname);
    fprintf(stderr, "  --in      filename    Name of InputOneCharPerBitFile\n");
    fprintf(stderr, "  --out     filename    Name of OutputModemRawFile\n");
    fprintf(stderr, "  --testframes Nsecs    Transmit test frames (adjusts test frames for raw and LDPC modes)\n");
    fprintf(stderr, "  --verbose  [1|2|3]    Verbose output level to stderr (default off)\n");
    fprintf(stderr, "  --text                Include a standard text message boolean (default off)\n");
    fprintf(stderr, "  --dpsk                Differential PSK (default off)\n");
    fprintf(stderr, "\n");
    exit(-1);
}

// Functions

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
 * Modulate the symbols by first upsampling to 8 kHz sample rate,
 * and translating the spectrum to 1100 Hz, where it is filtered
 * using the root raised cosine coefficients.
 */
static int tx_frame(int16_t frame[], complex float symbol[], int length, bool dpsk) {
    complex float signal[(length * CYCLES)];

    /*
     * Build the 1600 baud packet Frame zero padding
     * for the desired 8 kHz sample rate.
     */
    if (dpsk == true) {
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
        frame[i] = (int16_t) (crealf(signal[i]) * 16384.0f); // I at @ .5
    }
    
    return (length * CYCLES);
}

static int bpsk_pilot_modulate(int16_t frame[]) {
    return tx_frame(frame, pilot_table, PILOT_SYMBOLS, false);
}

static int qpsk_data_modulate(int16_t frame[], uint8_t tx_bits[], int index) {
    complex float symbol[DATA_SYMBOLS];
    int dibit[2];

    for (int i = 0, s = index; i < DATA_SYMBOLS; i++, s += 2) {
        dibit[0] = tx_bits[s + 1] & 0x1;
        dibit[1] = tx_bits[s ] & 0x1;

        symbol[i] = qpsk_mod(dibit);
    }

    return tx_frame(frame, symbol, DATA_SYMBOLS, dpsk_en);
}

int main(int argc, char *argv[]) {
    char *fin_name, *fout_name;
    int opt;

    char *pn = argv[0] + strlen(argv[0]);

    while (pn != argv[0] && !IS_DIR_SEPARATOR(pn[-1]))
        --pn;

    progname = pn;

    /* Turn off stream buffering */

    setvbuf(stdin, NULL, _IONBF, BUFSIZ);
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    FILE *fin = stdin;
    FILE *fout = stdout;
    
    struct optparse options;

    struct optparse_long longopts[] = {
        {"in", 'i', OPTPARSE_REQUIRED},
        {"out", 'o', OPTPARSE_REQUIRED},
        {"testframes", 'f', OPTPARSE_REQUIRED}, // TODO
        {"text", 't', OPTPARSE_NONE},           // TODO
        {"verbose", 'v', OPTPARSE_REQUIRED},       
        {"dpsk", 'd', OPTPARSE_NONE},           // TODO
        {"help", 'h', OPTPARSE_NONE},        
        {0, 0, 0}
    };

    optparse_init(&options, argv);

    while ((opt = optparse_long(&options, longopts, NULL)) != -1) {
        switch (opt) {
            case '?':
            case 'h':
                opt_help();
            case 'i':
                fin_name = options.optarg;
                input_specified = true;
                break;
            case 'o':
                fout_name = options.optarg;
                output_specified = true;
                break;
            case 'f':
                test_frames = true;
                Nsec = atoi(options.optarg);
                break;
            case 't':
                use_text = true;
                break;
            case 'd':
                dpsk_en = true;
                break;
            case 'v':
                verbose = atoi(options.optarg);
                if (verbose < 0 || verbose > 3)
                    verbose = 0;
        }
    }

    /* Print remaining arguments to give user a hint */

    char *arg;

    while ((arg = optparse_arg(&options)))
        fprintf(stderr, "%s\n", arg);

    if (input_specified) {
        if ((fin = fopen(fin_name, "rb")) == NULL) {
            fprintf(stderr, "Error opening input bits file: %s\n", fin_name);
            exit(-1);
        }
    }

    if (output_specified) {
        if ((fout = fopen(fout_name, "wb")) == NULL) {
            fprintf(stderr, "Error opening output modem sample file: %s\n", fout_name);
            exit(-1);
        }
    }
    
    /*
     * Create a complex table of pilot values
     * for the correlation algorithm
     */
    for (int i = 0; i < PILOT_SYMBOLS; i++) {
        pilot_table[i] = (float) pilotvalues[i]; // complex -1.0 or 1.0
    }

    fbb_tx_phase = cmplx(0.0f);
    fbb_tx_rect = cmplx(TAU * CENTER / FS);

    while (fread(bits, sizeof (uint8_t), BITS_PER_FRAME, fin) == BITS_PER_FRAME) {
        // 33 BPSK 1-bit pilots
        int length = bpsk_pilot_modulate(frame);

        fwrite(frame, sizeof (int16_t), length, fout);

        /*
         * NS data frames between each pilot frame
         */
        for (int i = 0; i < NS; i++) {
            // 31 QPSK 2-bit

            length = qpsk_data_modulate(frame, bits, (DATA_SYMBOLS * i) * 2);

            fwrite(frame, sizeof (int16_t), length, fout);
        }
    }

    if (input_specified)
        fclose(fin);

    if (output_specified)
        fclose(fout);

    return 0;
}
