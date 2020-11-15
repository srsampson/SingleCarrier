/*
 * scramble.c
 * 
 * November 2020
 * 
 * Note: This is not a Cryptographic Scrambler, it is a Bit Scrambler.
 *
 * This Linear Feedback Shift Register is taken from the Digital Video
 * Broadcast System (DVB), which uses the polynomial 1 + X14 + X15.
 *
 * The Sync Seed is reset at the start of each frame transmitted. 
 *
 *          In-Place 15 bit additive scrambler with 0x4a80 Frame Sync
 * 
 *  Sync    1   0   0   1   0   1   0   1   0   0   0   0   0   0   0
 *        +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *        | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10| 11| 12| 13| 14| 15|
 *        +-+-+---+---+---+---+---+---+---+---+---+---+---+---+-+-+-+-+
 *          ^                                                   |   |
 *          |                                                   v   v
 *          |                                                 +-------+
 *          +<------------------------------------------------|   +   |
 *          |                                                 +-------+ 
 *          v
 *        +---+
 * in --->| + |---> out
 *        +---+
 *
 * The scrambler is important for modems that don't use NRZI or bit-stuffing
 */

#include <stdint.h>

#include "qpsk.h"
#include "scramble.h"

// Globals

extern MCB mcb;

// Locals

static uint16_t scrambleTXMemory;
static uint16_t scrambleRXMemory;

// Functions

void resetTXScrambler() {
    scrambleTXMemory = SEED;
}

void resetRXScrambler() {
    scrambleRXMemory = SEED;
}
/* 
 * Functions duplicated for Full-Duplex operation.
 */
uint8_t scrambleTX(uint8_t input) {
    if (mcb.scramble == true) {
        for (int i = 0; i < BITS; i++) {
            uint16_t scrambler_out = (uint16_t) (((scrambleTXMemory & 0x2) >> 1) ^ (scrambleTXMemory & 0x1));
            uint16_t bits = (uint16_t) (((input >> i) & 0x1) ^ scrambler_out);

            input &= ~(1 << i);
            input |= (bits << i);

            /* update scrambler memory */
            scrambleTXMemory >>= 1;
            scrambleTXMemory |= (scrambler_out << 14);
        }
    }

    return input;
}

uint8_t scrambleRX(uint8_t input) {
    if (mcb.scramble == true) {
        for (int i = 0; i < BITS; i++) {
            uint16_t scrambler_out = (uint16_t) (((scrambleRXMemory & 0x2) >> 1) ^ (scrambleRXMemory & 0x1));
            uint16_t bits = (uint16_t) (((input >> i) & 0x1) ^ scrambler_out);

            input &= ~(1 << i);
            input |= (bits << i);

            /* update scrambler memory */
            scrambleRXMemory >>= 1;
            scrambleRXMemory |= (scrambler_out << 14);
        }
    }

    return input;
}