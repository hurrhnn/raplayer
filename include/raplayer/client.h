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
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <pthread.h>
#include <opus/opus.h>
#include <raplayer/config.h>
#include <raplayer/task_queue.h>

#ifndef RAPLAYER_RA_CLIENT_H
#define RAPLAYER_RA_CLIENT_H

typedef struct {
    struct {
        char* address;
        int port;
        void(*frame_callback) (void *frame, int frame_size, void* user_data);
        void* callback_user_data;

        int sock_fd;
        struct sockaddr_in server_addr;
        struct {
            uint16_t channels;
            int32_t sample_rate;
            uint16_t bit_per_sample;
        };
        uint8_t status;
        pthread_t thread;
    } *list;
    uint64_t idx;
} ra_client_t;

void* ra_client(void *p_client);

#endif
