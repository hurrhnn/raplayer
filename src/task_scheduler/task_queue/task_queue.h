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

#ifndef OPUSSTREAMER_SERVER_TASK_QUEUE_H
#define OPUSSTREAMER_SERVER_TASK_QUEUE_H

#include "client/client.h"
#include "task/task.h"

#define MAX_QUEUE_SIZE 0x7fff

typedef struct {
    int sock_fd;
    Client *client;

} TaskQueueInfo;

typedef struct {
    int front;
    int rear;

    TaskQueueInfo *queue_info;
    Task *tasks[MAX_QUEUE_SIZE];

} TaskQueue;

void init_queue(int sock_fd, Client *client, TaskQueue *q);

int is_full(const TaskQueue *q);

int is_empty(const TaskQueue *q);

bool append_task(TaskQueue *q, Task *task);

Task *perf_task(TaskQueue *q);

#endif
