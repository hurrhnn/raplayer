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

#include "task_scheduler.h"

_Noreturn void *schedule_task(void *p_task_scheduler_args) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);

    struct task_scheduler_info *task_scheduler_args = (struct task_scheduler_info *) p_task_scheduler_args;
    int sock_fd = task_scheduler_args->sock_fd;
    int *current_clients_count = task_scheduler_args->current_clients_count;
    TaskQueue **recv_queues = task_scheduler_args->recv_queues;

    while (true) {
        Task *task = calloc(sizeof(Task), BYTE);

        const struct sockaddr_in *p_client_addr = malloc(sizeof(struct sockaddr));
        struct sockaddr_in client_addr = *p_client_addr;

        const socklen_t *p_sock_len = calloc(sizeof(socklen_t), BYTE);
        socklen_t sock_len = *p_sock_len;
        sock_len = sizeof(client_addr);

        task->buffer_len = recvfrom(sock_fd, task->buffer, MAX_DATA_SIZE, 0, (struct sockaddr *) &client_addr, &sock_len);

        int client_id = -1;
        for (int i = 0; i < *current_clients_count; i++) {
            if ((strcmp(inet_ntoa(recv_queues[i]->queue_info->client->client_addr.sin_addr),
                        inet_ntoa(client_addr.sin_addr)) == 0) &&
                ntohs(recv_queues[i]->queue_info->client->client_addr.sin_port) == ntohs(client_addr.sin_port)) {
                client_id = i;
                break;
            }
        }

        if (client_id == -1) {
            (*current_clients_count) += 1;

            printf("\n%d: Connection from %s:%d\n", *current_clients_count, inet_ntoa(client_addr.sin_addr),
                   ntohs(client_addr.sin_port));
            fflush(stdout);

            recv_queues = realloc(recv_queues, sizeof(TaskQueue *) * (*current_clients_count));
            recv_queues[(*current_clients_count) - 1] = malloc(sizeof(TaskQueue));

            Client *client = malloc(sizeof(Client));
            client->client_addr = client_addr;
            client->socket_len = sock_len;

            init_queue(sock_fd, client, recv_queues[(*current_clients_count) - 1]);
            append_task(recv_queues[(*current_clients_count) - 1], task);

            pthread_mutex_lock(((struct task_scheduler_info *) p_task_scheduler_args)->complete_init_queue_mutex);
            pthread_cond_signal(((struct task_scheduler_info *) p_task_scheduler_args)->complete_init_queue_cond);
            pthread_mutex_unlock(((struct task_scheduler_info *) p_task_scheduler_args)->complete_init_queue_mutex);
        } else
            append_task(recv_queues[client_id], task);
    }
}
