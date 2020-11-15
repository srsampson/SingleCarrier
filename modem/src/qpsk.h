/*---------------------------------------------------------------------------*\

  FILE........: qpsk.h
  AUTHORS.....: David Rowe & Steve Sampson
  DATE CREATED: November 2020

  A QPSK modem Soundcard Application

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

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <complex.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h> 

/* manual/hard coded fine timing estimation for now */
#define FINE_TIMING_OFFSET 3.0f

#define FS              8000.0f
#define RS              1600.0f
#define CYCLES          (int) (FS / RS)
#define CENTER          1100.0f

#define NS                     8
#define PILOT_SYMBOLS          33
#define DATA_SYMBOLS           31
#define FRAME_SIZE             1405
#define SYMBOLS_PER_BLOCK      (FRAME_SIZE/4)

// (DATA_SYMBOLS * 2 bits * NS)
#define BITS_PER_FRAME  496
    
#define QUEUE_LENGTH           40
#define MAX_PACKET_LENGTH      4096     // some big number
#define MAX_NR_TX_SAMPLES      100000
#define EOF_COST_VALUE         5.0f

#define FEND                   0xC0
#define FESC                   0xDB
#define TFEND                  0xDC
#define TFESC                  0xDD
#define FFLAG                  0x7E
#define FFESC                  0x7D

#define NETWORK_PORT           33340
#define NETWORK_ADDR           "127.0.0.1"
    
#ifndef M_PI
#define M_PI            3.14159265358979323846f
#endif

#define TAU             (2.0f * M_PI)
#define ROT45           (M_PI / 4.0f)

/*
 * This method is much faster than using cexp() when real == 0
 * value - must be a float
 */
#define cmplx(value) (cosf(value) + sinf(value) * I)
#define cmplxconj(value) (cosf(value) + sinf(value) * -I)

/* modem state machine states */

typedef enum
{
    HUNT,
    PROCESS
} State;

/* Data block */

typedef struct
{
    size_t length;
    uint8_t *data;
} DBlock;

typedef struct
{
    complex float rx_scramble_symb;
    complex float rx_symb;
    float cost;
    float error;
    uint8_t data;
    uint8_t tx_symb;
} Rxed;

typedef enum
{
    NEW_FRAME,
    DATA,
    ESCAPE
} MdmState;

typedef enum
{
    TRANSMIT,
    RECEIVE
} PttState;

typedef enum
{
    RTS,
    DTR
} PttType;

/* Modem Control Block, overall control of the system */

typedef struct
{
    int fd; // Sound descriptor
    int sockfd; // Network descriptor
    int td; // PTT descriptor
    size_t sample_count;
    int sample_rate;
    int center_freq;
    int duplex;
    int tx_delay;
    int tx_tail;

    PttState ptt_state;
    PttType ptt_type;

    State rx_state;
} MCB;

// Prototypes

void intHandler(int);

int qpsk_pilot_modulate(int16_t []);
int qpsk_data_modulate(int16_t [], uint8_t [], int);
int qpsk_raw_modulate(uint8_t);

int qpsk_get_number_of_pilot_bits(void);
int qpsk_get_number_of_data_bits(void);

void qpsk_rx_freq_shift(complex float [], complex float [], int, int, float, complex float);
void qpsk_rx_frame(int16_t [], uint8_t []);
void qpsk_rx_end(void);

void reset_tx_scrambler(void);
void reset_rx_scrambler(void);
uint8_t tx_scramble(uint8_t);
void tx_packet(DBlock **, int);
uint8_t rx_descramble(uint8_t);

int packet_create(void);
void packet_destroy(void);
void packet_reset(void);
void packet_dibit_push(uint8_t);
DBlock *packet_pop(void);

int network_create(void);
void network_destroy(void);
DBlock *network_pop(void);
void network_kiss_read(void);
void network_kiss_write(uint8_t [], size_t);

// TODO
void ptt_transmit(void);
void ptt_receive(void);
void ptt_poll(void);

void end_of_rx_frame(void);
int16_t getAudioPeak(void);

#ifdef __cplusplus
}
#endif