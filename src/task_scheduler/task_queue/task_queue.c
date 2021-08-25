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

#include "../../ra_server.h"

void init_queue(int sock_fd, Client *client, TaskQueue *q) {
    q->rear = 0;
    q->front = 0;

    q->queue_info = malloc(sizeof(TaskQueueInfo));
    q->queue_info->sock_fd = sock_fd;
    q->queue_info->client = client;
}

int is_empty(const TaskQueue *q) {
    return (q->front == q->rear);
}

int is_full(const TaskQueue *q) { return ((q->rear + 1) % MAX_QUEUE_SIZE == q->front); }

bool append_task(TaskQueue *q, Task *task) {
    if (is_full(q)) { return false; }
    q->rear = (q->rear + 1) % MAX_QUEUE_SIZE;
    q->tasks[q->rear] = task;
    return true;
}

Task *perf_task(TaskQueue *q) {
    q->front = (q->front + 1) % MAX_QUEUE_SIZE;
    return q->tasks[q->front];
}
