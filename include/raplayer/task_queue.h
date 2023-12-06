/*
 raplayer is a cross-platform remote audio player, written from the scratch.
 This file is part of raplayer.

 Copyright (C) 2021 Rhnn Hur (hurrhnn)

    raplayer is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <raplayer/config.h>
#include <raplayer/chacha20.h>

#ifndef RAPLAYER_TASK_QUEUE_H
#define RAPLAYER_TASK_QUEUE_H

typedef struct {
    void *data;
    uint32_t data_len;
} ra_task_t;

typedef struct {
    int front;
    int rear;

    ra_task_t *tasks[MAX_QUEUE_SIZE];
    pthread_mutex_t mutex;
    pthread_cond_t empty, fill;

} ra_task_queue_t;

typedef enum {
    RA_NODE_INITIATED = (1 << 0),
    RA_NODE_HEARTBEAT_RECEIVED = (1 << 1),
    RA_NODE_CONNECTED = (1 << 2)
} ra_node_status_t;

typedef struct {
    ra_node_status_t status;
    uint64_t node_id;
    int sock_fd;
    struct sockaddr_in node_addr;
    socklen_t socket_len;
    struct chacha20_context crypto_context;

    ra_task_queue_t *recv_queue;
    ra_task_queue_t *send_queue;
} ra_node_t;

void init_queue(ra_task_queue_t *q);

int is_full(const ra_task_queue_t *q);

int is_empty(const ra_task_queue_t *q);

bool append_task(ra_task_queue_t *q, ra_task_t *task);

ra_task_t *retrieve_task(ra_task_queue_t *q);

ra_task_t* create_task(uint32_t len);

void remove_task(ra_task_t *task);

#endif
