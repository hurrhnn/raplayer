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

struct opus_builder_args {
    int **server_status;
    FILE *fin;
    OpusEncoder *encoder;

    pthread_mutex_t *opus_builder_mutex;
    pthread_mutex_t *opus_sender_mutex;

    pthread_cond_t *opus_builder_cond;
    pthread_cond_t *opus_sender_cond;
    Task *opus_frame;
};

struct opus_sender_args {
    int **server_status;
    TaskQueue *recv_queue;
    Task *opus_frame;

    pthread_mutex_t *opus_sender_mutex;
    pthread_cond_t *opus_sender_cond;
};

int ra_server(int port, int fd, uint32_t len, int** status);

#endif
