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

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <sys/poll.h>

#ifndef RAPLAYER_SCHEDULER_H
#define RAPLAYER_SCHEDULER_H

#include "queue.h"
#include "media.h"
#include "node.h"

typedef struct {
    struct pollfd **fds;
    uint64_t *cnt_fds;

    uint64_t *cnt_local_sock;
    ra_sock_local_t ***local_sock;

    uint64_t *cnt_remote_sock;
    ra_sock_remote_t ***remote_sock;

    ra_node_t ***node;
    uint64_t *cnt_node;

    ra_media_t ***media;
    uint64_t *cnt_media;
    pthread_mutex_t *media_mutex;
} ra_packet_scheduler_args_t;

_Noreturn void *schedule_packet(void *);

#endif
