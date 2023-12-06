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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <opus/opus.h>

#ifndef RAPLAYER_RA_SERVER_H
#define RAPLAYER_RA_SERVER_H

#include <raplayer/task_scheduler.h>
#include <raplayer/task_queue.h>
#include <raplayer/chacha20.h>

typedef struct {
    int *status, *is_sender_ready, *client_count, *turn, is_stream_mode;
    uint8_t *(*frame_callback) (void* user_data);
    void* callback_user_data;
    OpusEncoder *encoder;

    ra_node_t **client_context;

    pthread_mutex_t *opus_builder_mutex;
    pthread_cond_t *opus_builder_cond;
    pthread_mutex_t *complete_init_client_mutex;
    pthread_cond_t *complete_init_client_cond;

    pthread_rwlock_t *client_context_rwlock;
} opus_builder_args_t;

typedef struct {
    int *status, client_id;
    ra_node_t **client_context;

    pthread_mutex_t *opus_sender_mutex;
    pthread_cond_t *opus_sender_cond;

    pthread_rwlock_t *client_context_rwlock;
} opus_sender_args_t;

typedef struct {
    int *status, *turn, is_stream_mode;

    pthread_mutex_t *opus_builder_mutex;
    pthread_cond_t *opus_builder_cond;
    pthread_mutex_t *opus_sender_mutex;
    pthread_cond_t *opus_sender_cond;
    pthread_mutex_t *complete_init_client_mutex;
    pthread_cond_t *complete_init_client_cond;

    pthread_rwlock_t *client_context_rwlock;
} opus_timer_args_t;

typedef struct {
    struct {
        int port;
        uint8_t *(*frame_callback) (void* user_data);
        void* callback_user_data;
        int sock_fd;
        struct sockaddr_in server_addr;
        int status;
        pthread_t thread;
    } *list;
    uint64_t idx;
} ra_server_t;

void *ra_server(void *p_server);

#endif
