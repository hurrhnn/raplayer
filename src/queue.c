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

#include <string.h>
#include "raplayer/queue.h"
#include "raplayer/utils.h"

void init_queue(ra_queue_t *q, uint64_t size) {
    q->rear = 0;
    q->front = 0;
    q->size = (size == 0 ? RA_MAX_QUEUE_SIZE : size + 1);

    q->tasks = malloc(sizeof(ra_task_t *) * q->size);
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->empty, NULL);
    pthread_cond_init(&q->fill, NULL);
}

void destroy_queue(ra_queue_t *q) {
    free(q->tasks);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->empty);
    pthread_cond_destroy(&q->fill);
}

int is_empty(const ra_queue_t *q) {
    return (q->front == q->rear);
}

int is_full(const ra_queue_t *q) {
    return ((q->rear + 1) % q->size == q->front);
}

ra_task_t *create_task(uint32_t len) {
    ra_task_t *task = malloc(sizeof(ra_task_t));
    task->data = malloc(len);
    task->data_len = len;
    return task;
}

bool enqueue_task_with_removal(ra_queue_t *q, ra_task_t *task) {
    pthread_mutex_lock(&q->mutex);

    while (is_full(q)) {
        q->front = (q->front + 1) % q->size;
        ra_task_t *removed_task = q->tasks[q->front];
//        destroy_task(removed_task);
    }

    q->rear = (q->rear + 1) % q->size;
    q->tasks[q->rear] = task;

    pthread_cond_signal(&q->fill);
    pthread_mutex_unlock(&q->mutex);
    return true;
}

bool enqueue_task(ra_queue_t *q, ra_task_t *task) {
    pthread_mutex_lock(&q->mutex);
    while (is_full(q)) { pthread_cond_wait(&q->empty, &q->mutex); }
    q->rear = (q->rear + 1) % q->size;
    q->tasks[q->rear] = task;
    pthread_cond_signal(&q->fill);
    pthread_mutex_unlock(&q->mutex);
    return true;
}

ra_task_t *dequeue_task(ra_queue_t *q) {
    pthread_mutex_lock(&q->mutex);
    while (is_empty(q)) { pthread_cond_wait(&q->fill, &q->mutex); }
    q->front = (q->front + 1) % q->size;
    ra_task_t *current_task = q->tasks[q->front];
    pthread_cond_signal(&q->empty);
    pthread_mutex_unlock(&q->mutex);
    return current_task;
}

uint64_t get_size(ra_queue_t *q) {
    uint64_t size;
    if (q->front <= q->rear) {
        size = (int64_t) (q->rear - q->front);
    } else {
        size = (int64_t) ((q->rear + q->size - q->front) % q->size);
    }
    return size;
}

ra_task_t *retrieve_task(ra_queue_t *q, int64_t index, bool blocking) {
    pthread_mutex_lock(&q->mutex);
    uint64_t size = get_size(q);

    while (true) {
        if (is_empty(q)) {
            if(blocking) {
                pthread_cond_wait(&q->fill, &q->mutex);
                continue;
            } else {
                pthread_mutex_unlock(&q->mutex);
                return NULL;
            }
        }

        if (index == -1) {
            index = (int64_t) q->rear;
            break;
        } else {
            if(blocking) {
                while (index + 1 > size) {
                    pthread_cond_wait(&q->fill, &q->mutex);
                    size = get_size(q);
                }
            } else {
                if(index + 1 > size) {
                    pthread_mutex_unlock(&q->mutex);
                    return NULL;
                }
            }
            index++;
            break;
        }
    }

    ra_task_t *last_task = create_task(q->tasks[index]->data_len);
    memcpy(last_task->data, q->tasks[index]->data, last_task->data_len);

    pthread_mutex_unlock(&q->mutex);
    return last_task;
}

void destroy_task(ra_task_t *task) {
    free(task->data);
    free(task);
}