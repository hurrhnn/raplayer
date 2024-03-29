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

#ifndef RAPLAYER_TASK_QUEUE_H
#define RAPLAYER_TASK_QUEUE_H

typedef struct {
    void *data;
    uint32_t data_len;
} ra_task_t;

typedef struct {
    uint64_t front;
    uint64_t rear;

    uint64_t size;
    ra_task_t **tasks;
    pthread_mutex_t mutex;
    pthread_cond_t empty, fill;

} ra_queue_t;

void init_queue(ra_queue_t *q, uint64_t size);

int is_full(const ra_queue_t *q);

int is_empty(const ra_queue_t *q);

bool enqueue_task(ra_queue_t *q, ra_task_t *task);

bool enqueue_task_with_removal(ra_queue_t *q, ra_task_t *task);

ra_task_t *dequeue_task(ra_queue_t *q);

ra_task_t* create_task(uint32_t len);

ra_task_t *retrieve_task(ra_queue_t *q, int64_t index, bool blocking);

void destroy_task(ra_task_t *task);

#endif
