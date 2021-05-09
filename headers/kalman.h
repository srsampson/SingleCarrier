/*
 * kalman.h
 *
 * Square Root Kalman Filtering for High Speed Data Received
 * over Fading Dispersive Channels.
 * 
 * Transactions on Information Theory, Vol IT-28, No 5
 * 
 * Frank M. Hsu, Member IEEE
 * 
 * Copyright (C) September 1982
 * All Rights Reserved
 */

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

// Defines

#define EQ_LENGTH           5

// Prototypes

void init_kalman(void);
void kalman_reset(void);
void kalman_update(complex float [], complex float [], int, complex float);
void display_filter(complex float *);

#ifdef __cplusplus
}
#endif
