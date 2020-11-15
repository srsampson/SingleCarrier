/*---------------------------------------------------------------------------*\

  FILE........: fifo.c
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include "qpsk.h"
#include "fifo.h"

// Functions

Queue *create_fifo(size_t length) {
    if (length == 0)
        return (Queue *) NULL;  // Doh!

    Queue *queue = (Queue *) calloc(1, sizeof (Queue));

    if (queue != NULL) {
        queue->max_length = length - 1U;
        queue->length = 0;

        if ((queue->queue = (DBlock **) calloc(length, sizeof(DBlock *))) == NULL)
            return ((Queue *) NULL);

        queue->head_pointer = 0;
        queue->tail_pointer = 0;
        queue->state = FIFO_EMPTY;
    }
    
    return queue;
}

void fifo_destroy(Queue *queue) {
    if (queue != NULL) {
        free(queue->queue);
        free(queue);
    }
}

/*
 * You must check if FIFO is full before calling
 * this function
 */
void push_fifo(Queue *queue, DBlock item[], int index) {
    queue->queue[queue->head_pointer] = &item[index];
    queue->head_pointer++;
    queue->length++;

    if (queue->head_pointer > queue->max_length) {
        queue->head_pointer = 0;
    }

    if (queue->head_pointer == queue->tail_pointer) {
        queue->state = FIFO_FULL;
    } else {
        queue->state = FIFO_DATA;
    }
}

/*
 * You must check if FIFO is empty before calling
 * this function
 */
DBlock *pop_fifo(Queue *queue) {
    DBlock *item;

    if ((queue->head_pointer == queue->tail_pointer) && queue->state == FIFO_EMPTY) {
        /* The queue is empty */
        return ((DBlock *) NULL);
    }

    item = queue->queue[queue->tail_pointer];
    
    queue->tail_pointer++;
    queue->length--; /* One less item on the queue */
    
    if (queue->tail_pointer > queue->max_length) {
        queue->tail_pointer = 0; /* queue has rolled over */
    }
    
    if (queue->head_pointer == queue->tail_pointer) {
        queue->state = FIFO_EMPTY;
    } else {
        queue->state = FIFO_DATA;
    }

    return item;
}