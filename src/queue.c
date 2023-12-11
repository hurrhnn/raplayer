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

#include "raplayer/queue.h"

void init_queue(ra_queue_t *q) {
    q->rear = 0;
    q->front = 0;

    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->empty, NULL);
    pthread_cond_init(&q->fill, NULL);
}

int is_empty(const ra_queue_t *q) {
    return (q->front == q->rear);
}

int is_full(const ra_queue_t *q) {
    return ((q->rear + 1) % RA_MAX_QUEUE_SIZE == q->front);
}

ra_task_t* create_task(uint32_t len) {
    ra_task_t *task = malloc(sizeof(ra_task_t));
    task->data = malloc(len);
    task->data_len = len;
    return task;
}

bool append_task(ra_queue_t *q, ra_task_t *task) {
    pthread_mutex_lock(&q->mutex);
    while (is_full(q)) { pthread_cond_wait(&q->empty, &q->mutex); }
    q->rear = (q->rear + 1) % RA_MAX_QUEUE_SIZE;
    q->tasks[q->rear] = task;
    pthread_cond_signal(&q->fill);
    pthread_mutex_unlock(&q->mutex);
    return true;
}

ra_task_t *retrieve_task(ra_queue_t *q) {
    pthread_mutex_lock(&q->mutex);
    while (is_empty(q)) { pthread_cond_wait(&q->fill, &q->mutex); }
    q->front = (q->front + 1) % RA_MAX_QUEUE_SIZE;
    ra_task_t *current_task = q->tasks[q->front];
    pthread_cond_signal(&q->fill);
    pthread_mutex_unlock(&q->mutex);
    return current_task;
}

void remove_task(ra_task_t *task) {
    free(task->data);
    free(task);
}