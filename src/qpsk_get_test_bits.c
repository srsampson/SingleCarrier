/*---------------------------------------------------------------------------*\

  FILE........: qpsk_get_test_bits.c
  AUTHORS.....: David Rowe & Steve Sampson
  DATE CREATED: October 2020

  Generate input for the QPSK modem.

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

#define OPTPARSE_IMPLEMENTATION
#define OPTPARSE_API static
#include "optparse.h"

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>

#include "qpsk_internal.h"

#define IS_DIR_SEPARATOR(c) ((c) == '/')

static uint8_t bits[BITS_PER_FRAME];
    
static const char *progname;

void opt_help() {
    fprintf(stderr, "\nUsage: %s [options]\n\n", progname);
    fprintf(stderr, "  --out     filename  Name of OutputOneCharPerBitFile\n");
    fprintf(stderr, "  --frames  n         Number of frames to output (default 10)\n\n");

    exit(-1);
}

int main(int argc, char *argv[]) {
    char *fout_name;
    int opt;

    srand(time(0));
    
    char *pn = argv[0] + strlen(argv[0]);

    while (pn != argv[0] && !IS_DIR_SEPARATOR(pn[-1]))
        --pn;

    progname = pn;

    /* Turn off stream buffering */

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    FILE *fout = stdout;
    bool output_specified = false;
    int frames = 10;

    struct optparse options;

    struct optparse_long longopts[] = {
        {"out", 'o', OPTPARSE_REQUIRED},
        {"frames", 'f', OPTPARSE_REQUIRED},
        {0, 0, 0}
    };

    optparse_init(&options, argv);

    while ((opt = optparse_long(&options, longopts, NULL)) != -1) {
        switch (opt) {
            case '?':
                opt_help();
            case 'o':
                fout_name = options.optarg;
                output_specified = true;
                break;
            case 'f':
                frames = atoi(options.optarg);
        }
    }

    /* Print remaining arguments to give user a hint */

    char *arg;

    while ((arg = optparse_arg(&options)))
        fprintf(stderr, "%s\n", arg);

    if (output_specified) {
        if ((fout = fopen(fout_name, "wb")) == NULL) {
            fprintf(stderr, "Error opening output bit file: %s\n", fout_name);
            exit(-1);
        }
    }

    int Nframes = frames;

    for (int i = 0; i < Nframes; i++) {
        for (int j = 0; j < BITS_PER_FRAME; j++) {
            bits[j] = rand() % 2;
        }

        fwrite(bits, sizeof (char), BITS_PER_FRAME, fout);
    }

    if (output_specified)
        fclose(fout);

    return 0;
}
