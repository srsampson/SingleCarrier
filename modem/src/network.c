#define DEBUG
/*---------------------------------------------------------------------------*\

  FILE........: network.c
  AUTHORS.....: David Rowe & Steve Sampson
  DATE CREATED: November 2020

  A QPSK modem network interface

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
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>

#include "qpsk.h"
#include "fifo.h"

// Externals

extern MCB mcb;
extern bool running;
extern Queue *packet_queue;
extern pthread_mutex_t packet_lock;

// Prototypes

static void kiss_control(uint8_t []);
static void *socketReadThread(void *);
static void *socketWriteThread(void *);
static void *transmitThread(void *);

// Globals

Queue *network_queue;
pthread_mutex_t network_lock = PTHREAD_MUTEX_INITIALIZER;

// Locals

static uint8_t msg[QUEUE_LENGTH][MAX_PACKET_LENGTH];
static DBlock dataBlock[QUEUE_LENGTH];
static DBlock *inblock[QUEUE_LENGTH];

static pthread_t rid;
static pthread_t wid;
static pthread_t tid;

static int dbp;
static int msg_counter;
static int serverSocket;

static bool esc_flag;

// Functions

/*
 * Receive data on socket and push onto queue
 */
static void *socketReadThread(void *arg) {
    uint8_t octet;
    ssize_t count;

    /*
     * Allow for Control-C escape with running boolean
     */
    while (running == true) {

        /* read from socket */
        count = recv(mcb.sockfd, &octet, 1, 0);
        
        /*
         * See if client closed
         */
        if (count == 0) {
            running = false;
            break;
        } else if (count < 0) {
            sleep(1);
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
                        if (msg[dbp][0] == 0) {
                            /*
                             * Save block of data on the queue
                             * First octet is KISS command, so use 1
                             */
                            dataBlock[dbp].data = &msg[dbp][1];
                            dataBlock[dbp].length = (msg_counter - 1);


                            
                            if (network_queue->state != FIFO_FULL) {
                                pthread_mutex_lock(&network_lock);
                                
                                push_fifo(network_queue, dataBlock, dbp);

#ifdef DEBUG
    for (int i = 0; i < dataBlock[dbp].length; i++) {
        fprintf(stderr, "%02X", dataBlock[dbp].data[i]);
    }
    fprintf(stderr, "\n");
#endif
                                dbp = (dbp + 1) % QUEUE_LENGTH;
                                
                                pthread_mutex_unlock(&network_lock);
                            } else {
                                fprintf(stderr, "Warning: network queue overrun\n");
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
                } else {
                    /*
                     * If not FEND Keep adding octets to
                     * the frame and increment counter
                     */
                    if (msg_counter == 0) {
                        /*
                         * Upper nibble on command octet problematic
                         */
                        msg[dbp][msg_counter++] = (octet & 0xF);
                    } else {
                        if (octet == FESC) {
                            esc_flag = true;
                        } else {
                            msg[dbp][msg_counter++] = octet;
                        }
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
    
    pthread_exit(NULL);
}

static void *transmitThread(void *arg) {
    int loop = 0;
    
    /*
     * Allow for Control-C escape with running boolean
     */
    while (running == true) {
        if (network_queue->state != FIFO_EMPTY) {
            pthread_mutex_lock(&network_lock);
            
            if ((inblock[loop++] = (DBlock *) pop_fifo(network_queue)) != NULL) {

                while ((inblock[loop] = pop_fifo(network_queue)) != NULL)
                    loop++;

                tx_packet(inblock, loop);
            }
            
            pthread_mutex_unlock(&network_lock);
            loop = 0;
        }
        
      
        
        sleep(1);
    }

    pthread_exit(NULL);
}

/*
 * Now process any decoded modem input data
 *
 * Returns Data Block from network socket
 * containing the KISS encapsulated data
 * to be sent to the modem transmitter
 *
 */
static void *socketWriteThread(void *arg) {
    DBlock *dblock;
    uint8_t frame[2];

    /*
     * Allow for Control-C escape with running boolean
     */
    while (running == true) {
        while (packet_queue->state != FIFO_EMPTY) {
            pthread_mutex_lock(&packet_lock);
            
            dblock = pop_fifo(packet_queue);
            
            pthread_mutex_unlock(&packet_lock);
            
            /*
             * Send packets to the AX.25 KISS network
             */
            frame[0] = FEND;
            frame[1] = 0;

            send(mcb.sockfd, frame, 2, 0);

            /*
             * Encode any Framing octets while sending data
             * to the AX.25 network using the network socket
             */
            for (size_t i = 0; i < dblock->length; i++) {
                if (dblock->data[i] == FEND) {
                    frame[0] = FESC;
                    frame[1] = TFEND;
                    send(mcb.sockfd, frame, 2, 0);
                } else if (dblock->data[i] == FESC) {
                    frame[0] = FESC;
                    frame[1] = TFESC;
                    send(mcb.sockfd, frame, 2, 0);
                } else {
                    /*
                     * raw AX.25 data
                     */
                    send(mcb.sockfd, &dblock->data[i], 1, 0);
                }
            }

            frame[0] = FEND;

            send(mcb.sockfd, frame, 1, 0);
        }
        
        sleep(1);
    }
    
    pthread_exit(NULL);
}

/*
 * TCP/IP network code
 */
int network_create() {
    /*
     * Create a queue
     */
    network_queue = create_fifo(QUEUE_LENGTH);

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket < 0) {
        fprintf(stderr, "Could not open server socket\n");
        return -1;
    }
    
    struct sockaddr_in serverAddr;
    struct sockaddr_in client;
    
    memset(&serverAddr, 0, sizeof(serverAddr));

    // assign IP, PORT 
    serverAddr.sin_family = AF_INET; 
    serverAddr.sin_addr.s_addr = inet_addr(NETWORK_ADDR); 
    serverAddr.sin_port = htons(NETWORK_PORT);

    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        fprintf(stderr, "Fatal: TCP socket bind fail\n");
        return -1;
    }

    /*
     * Only 1 connection
     */
    if ((listen(serverSocket, 1)) != 0) { 
        fprintf(stderr, "Fatal: TCP socket listen failed\n"); 
        return -1; 
    }
    
    /*
     * OK, we have a connect
     */
    socklen_t client_length = sizeof(client);

    int sockfd = accept(serverSocket, (struct sockaddr *)&client, &client_length);

    if (sockfd < 0) { 
        fprintf(stderr, "Fatal: Server accept failed\n"); 
        return -1; 
    } else {
        fprintf(stderr, "\nConnect...\n");
    }

    mcb.sockfd = sockfd;

    if (pthread_create(&rid, NULL, socketReadThread, NULL) != 0) {
        fprintf(stderr, "Fatal: Failed to create read socket thread\n");
        return -1;
    }
    
    if (pthread_create(&wid, NULL, socketWriteThread, NULL) != 0) {
        fprintf(stderr, "Fatal: Failed to create write socket thread\n");
        return -1;
    }
    
    if (pthread_create(&tid, NULL, transmitThread, NULL) != 0) {
        fprintf(stderr, "Fatal: Failed to create transmit thread\n");
        return -1;
    }

    dbp = 0;
    esc_flag = false;
    msg_counter = 0;

    return 0;
}

void network_destroy() {
    pthread_join(wid, NULL);
    pthread_join(rid, NULL);
    pthread_join(tid, NULL);
    
    fifo_destroy(network_queue);
    close(mcb.sockfd);
    close(serverSocket);
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
    }
}
