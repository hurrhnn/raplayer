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

#ifndef RAPLAYER_TASK_SCHEDULER_H
#define RAPLAYER_TASK_SCHEDULER_H

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <raplayer/task_queue.h>
#include <raplayer/chacha20.h>

typedef struct {
    int sock_fd;
    int *client_count;
    ra_node_t **client_context;

    pthread_mutex_t *complete_init_queue_mutex;
    pthread_cond_t *complete_init_queue_cond;

    pthread_rwlock_t *client_context_rwlock;
} task_scheduler_info_t;

typedef struct {
    uint8_t *status;
    int *client_count;
    ra_node_t **client_context;

    uint32_t data_len;
    int is_sender_ready;

    pthread_mutex_t *complete_init_mutex[2];
    pthread_mutex_t *opus_sender_mutex;

    pthread_cond_t *complete_init_cond[2];
    pthread_cond_t *opus_sender_cond;

    pthread_rwlock_t *client_context_rwlock;
} client_handler_args_t;

_Noreturn void *schedule_task(void *p_task_scheduler_args);

#endif
