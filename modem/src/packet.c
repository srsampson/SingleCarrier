/*---------------------------------------------------------------------------*\

  FILE........: packet.c
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

#include <stdio.h>
#include <stdlib.h>

#include "qpsk.h"
#include "crc.h"
#include "fifo.h"

// Prototypes

static void packet_push(size_t);

// Globals

Queue *packet_queue;

// Locals

static MdmState n_state;

static size_t dibit_count;
static size_t octet_count;

static uint8_t packet[MAX_PACKET_LENGTH];
static uint8_t octet;

// Functions

/*
 * An 8-bit octet is made-up of four dibits
 */
int packet_create() {
    packet_queue = create_fifo(QUEUE_LENGTH);
    
    if (packet_queue == (Queue *) NULL) {
        fprintf(stderr, "Fatal: packet_create Unable to create Packet Queue\n");
        return -1;
    }
    
    return 0;
}

void packet_destroy() {
    delete_fifo(packet_queue);
}

void packet_reset() {
    dibit_count = 0;
    octet_count = 0;
    octet = 0;
    
    n_state = NEW_FRAME;    
}

static void packet_push(size_t length) {
    DBlock *dblock = (DBlock *) malloc(sizeof (DBlock));
    dblock->data = (uint8_t *) calloc(length, sizeof(uint8_t));
    dblock->length = length;

    for (size_t i = 0; i < length; i++) {
        dblock->data[i] = packet[i];
    }
    
    if (packet_queue->state != FIFO_FULL) {
        push_fifo(packet_queue, dblock, 0);
    } else {
        /* Queue overflowed */
        free(dblock->data);
        free(dblock);
    }
}

DBlock *packet_pop() {
    return pop_fifo(packet_queue);
}

/*
 * Add in QPSK dibits from modem receiver
 * until we get an octet worth.
 */
void packet_dibit_push(uint8_t dibit) {
    dibit_count++;

    switch (dibit_count) {
        case 1:
            octet = dibit;
            break;
        case 2:
        case 3:
            octet = (octet << 2) | dibit;
            break;
        case 4:
            /*
             * OK, with 4th dibit, we have an octet to process
             */
            octet = (octet << 2) | dibit;

            /*
             * Process depending on state
             */
            switch (n_state) {
                case NEW_FRAME:
                    if (octet == FFLAG) {
                        octet_count = 0;
                        dibit_count = 0;
                        resetCRC();
                        n_state = DATA;
                    }
                    break;
                case DATA:
                    if (octet == FFLAG) {
                        if ((getCRC() == 0) && (octet_count > 2)) {
                            /* Good frame */

                            packet_push(octet_count - 2);
                        }
                        n_state = NEW_FRAME;
                    } else if (octet == FFESC) {
                        /* Throw octet away */
                        n_state = ESCAPE;
                    } else {
                        // TODO MAX Over-runs are not handled yet
                        if (octet_count < MAX_PACKET_LENGTH - 1) {
                            updateCRC(octet);
                            packet[octet_count++] = octet;
                            dibit_count = 0;
                        }
                    }
                    break;
                case ESCAPE:
                    // TODO MAX Over-runs are not handled yet
                    if (octet_count < MAX_PACKET_LENGTH - 1) {
                        octet = octet ^ 0x20;
                        updateCRC(octet);
                        packet[octet_count++] = octet;
                        dibit_count = 0;
                    }
                    
                    n_state = DATA;
            }
    }
}