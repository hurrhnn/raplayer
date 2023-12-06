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

#include <raplayer/config.h>
#include <raplayer/task_scheduler.h>
#include <raplayer/task_queue.h>

_Noreturn void *schedule_task(void *p_task_scheduler_args) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    task_scheduler_info_t *task_scheduler_args = (task_scheduler_info_t *) p_task_scheduler_args;
    int sock_fd = task_scheduler_args->sock_fd;
    int *client_count = task_scheduler_args->client_count;

    while (true) {
        ra_task_t *task = create_task(MAX_DATA_SIZE);
        memset(task->data, 0x0, task->data_len);

        struct sockaddr_in client_addr;
        socklen_t sock_len;
        sock_len = sizeof(client_addr);

        task->data_len = recvfrom(sock_fd, task->data, MAX_DATA_SIZE, 0, (struct sockaddr *) &client_addr,
                                  &sock_len);

        int client_id = -1;
        for (int i = 0; i < *client_count; i++) {
            pthread_rwlock_rdlock(task_scheduler_args->client_context_rwlock);
            ra_node_t *node = &(*task_scheduler_args->client_context)[i];
            if ((strcmp(inet_ntoa(node->node_addr.sin_addr), inet_ntoa(client_addr.sin_addr)) == 0) &&
                ntohs(node->node_addr.sin_port) == ntohs(client_addr.sin_port)) {
                client_id = i;
                pthread_rwlock_unlock(task_scheduler_args->client_context_rwlock);
                break;
            }
            pthread_rwlock_unlock(task_scheduler_args->client_context_rwlock);
        }

        if (client_id == -1) {
            (*client_count) += 1;
            printf("\n%02d: Connection from %s:%d\n", *client_count, inet_ntoa(client_addr.sin_addr),
                   ntohs(client_addr.sin_port));
            fflush(stdout);

            pthread_rwlock_wrlock(task_scheduler_args->client_context_rwlock);
            *task_scheduler_args->client_context = realloc(*task_scheduler_args->client_context,
                                                           sizeof(ra_node_t) * (*client_count));
            memset(*task_scheduler_args->client_context + sizeof(ra_node_t) * ((*client_count) - 1), 0x0,
                   sizeof(ra_node_t));
            pthread_rwlock_unlock(task_scheduler_args->client_context_rwlock);

            pthread_rwlock_rdlock(task_scheduler_args->client_context_rwlock);
            ra_node_t *node = &(*task_scheduler_args->client_context)[(*client_count) - 1];
            node->sock_fd = sock_fd;
            node->node_addr = client_addr;
            node->socket_len = sock_len;
            node->node_id = (*client_count) - 1;
            node->status = 0;
            node->recv_queue = malloc(sizeof(ra_task_queue_t));
            node->send_queue = malloc(sizeof(ra_task_queue_t));

            init_queue(node->recv_queue);
            init_queue(node->send_queue);
            append_task(node->recv_queue, task);
            pthread_rwlock_unlock(task_scheduler_args->client_context_rwlock);

            node->status |= RA_NODE_INITIATED;
            pthread_mutex_lock(task_scheduler_args->complete_init_queue_mutex);
            pthread_cond_signal(task_scheduler_args->complete_init_queue_cond);
            pthread_mutex_unlock(task_scheduler_args->complete_init_queue_mutex);
        } else {
            pthread_rwlock_rdlock(task_scheduler_args->client_context_rwlock);
            ra_node_t *node = &(*task_scheduler_args->client_context)[client_id];
            if (!strncmp((char *) task->data, HEARTBEAT, sizeof(HEARTBEAT))) {
                node->status |= RA_NODE_HEARTBEAT_RECEIVED;
                remove_task(task);
            } else
                append_task(node->recv_queue, task);
            pthread_rwlock_unlock(task_scheduler_args->client_context_rwlock);
        }
    }
}
