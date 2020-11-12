/*---------------------------------------------------------------------------*\

  FILE........: network.c
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

#define _XOPEN_SOURCE 600
#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <termios.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>

#include "qpsk.h"
#include "fifo.h"

// Externals

extern MCB mcb;

// Prototypes

static void kiss_control(uint8_t []);

// Globals

Queue *pseudo_queue;

// Locals

static uint8_t msg[QUEUE_LENGTH][MAX_PACKET_LENGTH];
static DBlock dataBlock[QUEUE_LENGTH];

static bool esc_flag;

static int dbp;
static int msg_counter;

static int masterfd;
static char *slavename;

// Functions

/*
 * Legacy AX.25 KISS Pseudo TTY network code
 */
int pseudo_create() {
    masterfd = open("/dev/ptmx", O_RDWR);

    if (masterfd == -1 || grantpt(masterfd) == -1 || unlockpt(masterfd) == -1 ||
            (slavename = ptsname(masterfd)) == NULL) {
        fprintf(stderr, "Unable to open Pseudo Terminal\n");
        return -1;
    }

    printf("Pseudo Terminal to connect with: %s\n", slavename);

    esc_flag = false;
    msg_counter = 0;

    pseudo_queue = create_fifo(QUEUE_LENGTH);

    if (pseudo_queue == (Queue *) NULL) {
        fprintf(stderr, "Fatal: pseudo_create unable to create Queue\n");
        return -1;
    }
    
    dbp = 0;
    
    mcb.pd = masterfd;
    
    return 0;
}

void pseudo_destroy() {
    delete_fifo(pseudo_queue);
    close(masterfd);
}

static void kiss_control(uint8_t msg[]) {
    switch (msg[0] & 0x0F) {
        case 1:
            /* TX Delay */
            mcb.tx_delay = msg[1];
            break;
        case 2:
            /* Persistence */
            break;
        case 3:
            /* Slot time */
            break;
        case 4:
            /* TX Tail */
            mcb.tx_tail = msg[1];
            break;
        case 5:
            /* Full Duplex */
            mcb.duplex = msg[1];
        default:
            break;
    }
}
/*
 * Poll the pseudo TTY input and push onto queue
 */
void pseudo_poll() {
    uint8_t octet;
    int status;

    while (1) {

        /* read the octet from Blocking pseudo TTY */
        status = read(mcb.pd, &octet, 1);

        /*
         * See if no data or error
         */
        if (status <= 0) {
            break;
        } else {
            /*
             * Process data if we have at least one character
             */
            if (esc_flag == false) {
                if (octet == FEND) {

                    /*
                     * See if it is end
                     */
                    if (msg_counter != 0) {

                        /*
                         * Must be an end
                         * 
                         * Check if KISS control block
                         * 0 == DATA
                         * Some KISS use the upper command nibble
                         * for encoding port number, etc. We don't
                         * care about that here.
                         */
                        if ((msg[dbp][0] & 0xF) == 0) {
                            /*
                             * Save block of data on the queue
                             * First octet is KISS command, so use 1
                             */
                            dataBlock[dbp].data = &msg[dbp][1];
                            dataBlock[dbp].length = (msg_counter - 1);

                            if (pseudo_queue->state != FIFO_FULL) {
                                push_fifo(pseudo_queue, dataBlock, dbp);
                                dbp = (dbp + 1) % QUEUE_LENGTH;
                            } else {
                                fprintf(stderr, "warning: pseudo queue overrun\n");
                            }
                        } else {
                            /*
                             * process any control blocks
                             */
                            kiss_control(msg[dbp]);
                        }
                    }
                    
                    // It's a start, or we just pushed to queue

                    msg_counter = 0;
                    break;
                } else {

                    /*
                     * If not FEND Keep adding octets to
                     * the frame and increment counter
                     */
                    if (octet == FESC) {
                        esc_flag = true;
                    } else {
                        msg[dbp][msg_counter++] = octet;
                    }
                }
            } else if (octet == TFESC) {    // esc_flag is true
                msg[dbp][msg_counter++] = FESC;
            } else if (octet == TFEND) {
                msg[dbp][msg_counter++] = FEND;
            }

            esc_flag = false;   // toggle back
        }
    }
}

/*
 * Returns Data Block containing network KISS data
 * to be sent to the modem transmitter
 */
DBlock *pseudo_listen() {
    return pop_fifo(pseudo_queue);
}

/*
 * Write data to the pseudo TTY
 */
void pseudo_write_kiss_control(uint8_t msg[], size_t length) {
    uint8_t data[2];

    data[0] = FEND;

    write(mcb.pd, data, 1);

    for (size_t i = 0; i < length; i++) {
        if (msg[i] == FEND) {
            data[0] = FESC;
            data[1] = TFEND;
            write(mcb.pd, data, 2);
        } else if (msg[i] == FESC) {
            data[0] = FESC;
            data[1] = TFESC;
            write(mcb.pd, data, 2);
        } else {
            write(mcb.pd, &msg[i], 1);
        }
    }

    data[0] = FEND;

    write(mcb.pd, data, 1);
}

/*
 * Set KISS Command byte to 0
 * 
 * The data is in the local static memory array msg[n][]
 * The queue merely stores a reference to it. There can
 * be at most QUEUE_LENGTH packets.
 */
void pseudo_write_kiss_data(uint8_t *data, size_t length) {
    uint8_t frame[2];

    frame[0] = FEND;
    frame[1] = 0;

    write(mcb.pd, frame, 2);

    /*
     * Encode any Framing octets while sending data
     * to the AX.25 network using the pseudo TTY
     */
    for (size_t i = 0; i < length; i++) {
        if (data[i] == FEND) {
            frame[0] = FESC;
            frame[1] = TFEND;
            write(mcb.pd, frame, 2);
        } else if (data[i] == FESC) {
            frame[0] = FESC;
            frame[1] = TFESC;
            write(mcb.pd, frame, 2);
        } else {
            /*
             * raw AX.25 data
             */
            write(mcb.pd, &data[i], 1);
        }
    }

    frame[0] = FEND;

    write(mcb.pd, frame, 1);
}
