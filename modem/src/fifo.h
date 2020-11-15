/*---------------------------------------------------------------------------*\

  FILE........: fifo.h
  AUTHORS.....: David Rowe & Steve Sampson
  DATE CREATED: November 2020

  A QPSK modem FIFO Queue

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

#include <stdint.h>
#include <stdbool.h>

#include "qpsk.h"

/* Queue control block and associated values */

typedef enum
{
    FIFO_DATA,
    FIFO_EMPTY,
    FIFO_FULL
} Queue_status;

typedef struct
{
    int    head_pointer;
    int    tail_pointer;

    size_t length; /* Number of items on the queue    */
    size_t max_length; /* Maximum offset into queue array */

    Queue_status state;
    
    DBlock **queue;
} Queue;

Queue *create_fifo(size_t);
void fifo_destroy(Queue *);

void push_fifo(Queue *, DBlock [], int);
DBlock *pop_fifo(Queue *);

#ifdef __cplusplus
}
#endif