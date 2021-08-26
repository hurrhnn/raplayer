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

#ifndef OPUSSTREAMER_SERVER_TASK_SCHEDULER_H
#define OPUSSTREAMER_SERVER_TASK_SCHEDULER_H

#include "../ra_server.h"
#include "task_queue/task_queue.h"

struct task_scheduler_info {;
    int sock_fd;
    int *current_clients_count;
    TaskQueue **recv_queues;

    pthread_mutex_t *complete_init_queue_mutex;
    pthread_cond_t *complete_init_queue_cond;
};

struct client_handler_info {
    int *current_clients_count;
    TaskQueue ***recv_queues;
    struct pcm *pcm_struct;
    unsigned char *crypto_payload;

    bool *stop_consumer;
    pthread_t *stream_consumer;

    pthread_mutex_t *complete_init_mutex[2];
    pthread_mutex_t *opus_sender_mutex;

    pthread_cond_t *complete_init_cond[2];
    pthread_cond_t *opus_sender_cond;

    Task *opus_frame;
};

_Noreturn void *schedule_task(void *p_task_scheduler_args);

#endif
