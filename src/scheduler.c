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
#include "raplayer/scheduler.h"
#include "raplayer/dispatcher.h"

_Noreturn void *schedule_packet(void *p_ra_packet_scheduler_args) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    pthread_t *frame_receiver = NULL;
    ra_packet_scheduler_args_t *ra_packet_scheduler_args = (ra_packet_scheduler_args_t *) p_ra_packet_scheduler_args;

    while (true) {
        struct pollfd *fds = *ra_packet_scheduler_args->fds;
        ra_sock_local_t **local_sock = *ra_packet_scheduler_args->local_sock;
        ra_sock_remote_t **remote_sock = *ra_packet_scheduler_args->remote_sock;
        ra_node_t **node = *ra_packet_scheduler_args->node;

        if (fds == NULL || *ra_packet_scheduler_args->cnt_fds == 0) {
            continue;
        }

        uint64_t cnt_fds = *ra_packet_scheduler_args->cnt_fds;
        if (poll(fds, cnt_fds, -1) == -1)
            continue;

        ra_task_t *task = create_task(RA_MAX_DATA_SIZE);
        memset(task->data, 0x0, RA_MAX_DATA_SIZE);

        struct sockaddr_in remote_addr;
        socklen_t sock_len;
        sock_len = sizeof(remote_addr);

        for (uint64_t i = 0; i < cnt_fds; i++) {
            if (fds[i].fd != -1 && fds[i].revents & POLLIN) {
                task->data_len = recvfrom(fds[i].fd, task->data, RA_MAX_DATA_SIZE, 0, (struct sockaddr *) &remote_addr,
                                          &sock_len);
                int node_id = -1;
                for (int j = 0; j < *ra_packet_scheduler_args->cnt_node; j++) {
                    if (node[j]->local_sock->fd == fds[i].fd &&
                        ra_compare_sockaddr(&node[j]->remote_sock->addr, &remote_addr)) {
                        node_id = j;
                        break;
                    }
                }

                if (node_id == -1) {
                    int local_sock_idx = -1;
                    for (int j = 0; j < *ra_packet_scheduler_args->cnt_local_sock; j++) {
                        if (fds[i].fd == local_sock[j]->fd) {
                            local_sock_idx = j;
                            break;
                        }
                    }

                    if (local_sock_idx == -1)
                        continue;

                    RA_INFO("%02llu: New Connection with %s:%d\n", (*ra_packet_scheduler_args->cnt_node) + 1,
                            inet_ntoa(remote_addr.sin_addr),
                            ntohs(remote_addr.sin_port));
//                            RA_DEBUG_MORE(GRN, "Node context write lock initiated.\n");
//                            pthread_rwlock_wrlock(task_scheduler_args->client_context_rwlock);
                    void *before_address = *ra_packet_scheduler_args->remote_sock;
                    *ra_packet_scheduler_args->remote_sock =
                            realloc(remote_sock, sizeof(ra_sock_remote_t *) *
                                                 ((*ra_packet_scheduler_args->cnt_remote_sock) + 1));
                    RA_DEBUG_MORE(GRN, "Reallocated remote sock context %p to %p, size: 0x%llX\n",
                                  before_address,
                                  *ra_packet_scheduler_args->remote_sock,
                                  sizeof(ra_sock_remote_t *) *
                                  ((*ra_packet_scheduler_args->cnt_remote_sock) + 1));
                    remote_sock = *ra_packet_scheduler_args->remote_sock;

                    remote_sock[*ra_packet_scheduler_args->cnt_remote_sock] = malloc(sizeof(ra_sock_remote_t));
                    remote_sock[*ra_packet_scheduler_args->cnt_remote_sock]->local_sock = local_sock[local_sock_idx];
                    memcpy(&remote_sock[*ra_packet_scheduler_args->cnt_remote_sock]->addr, &remote_addr,
                           sizeof(struct sockaddr_in));

                    before_address = *ra_packet_scheduler_args->node;
                    *ra_packet_scheduler_args->node =
                            realloc(node, sizeof(ra_node_t *) * ((*ra_packet_scheduler_args->cnt_node) + 1));
                    RA_DEBUG_MORE(GRN, "Reallocated node context %p to %p, size: 0x%llX\n", before_address,
                                  *ra_packet_scheduler_args->node,
                                  sizeof(ra_node_t *) * ((*ra_packet_scheduler_args->cnt_node) + 1));
                    node = *ra_packet_scheduler_args->node;

                    uint64_t id = *ra_packet_scheduler_args->cnt_node;
                    node[id] = malloc(sizeof(ra_node_t));
                    node[id]->id = id;
                    node[id]->status = RA_NODE_INITIATED;
                    node[id]->local_sock = local_sock[local_sock_idx];
                    node[id]->remote_sock = remote_sock[*ra_packet_scheduler_args->cnt_remote_sock];
                    node[id]->sample_rate = 48000;
                    node[id]->channels = 2;
                    node[id]->bit_per_sample = 16;
                    node[id]->recv_queue = malloc(sizeof(ra_queue_t));
                    node[id]->send_queue = malloc(sizeof(ra_queue_t));

                    ra_media_register_t media_register = {ra_packet_scheduler_args->media,
                                                          ra_packet_scheduler_args->cnt_media,
                                                          ra_packet_scheduler_args->media_mutex,
                                                          RA_MEDIA_TYPE_REMOTE_PROVIDE, 7};
                    uint64_t media_id = ra_media_register(media_register, NULL, NULL);

                    ra_media_t **media = *ra_packet_scheduler_args->media;
                    media[media_id]->src = local_sock[local_sock_idx];
                    media[media_id]->status = RA_MEDIA_INITIATED;

                    node[id]->remote_media = media[media_id];

                    init_queue(node[id]->recv_queue, 0);
                    init_queue(node[id]->send_queue, 0);
                    enqueue_task(node[id]->recv_queue, task);

//                            pthread_rwlock_unlock(task_scheduler_args->client_context_rwlock);
//                            RA_DEBUG_MORE(GRN, "Node context write lock released.\n");

                    node[id]->status = RA_NODE_INITIATED;

                    /* activate node packet dispatcher */
                    pthread_t packet_dispatcher;
                    pthread_create(&packet_dispatcher, NULL, dispatch_packet, node[id]);

                    pthread_t frame_sender;
                    ra_node_frame_args_t *node_frame_args = malloc(sizeof(ra_node_frame_args_t));
                    node_frame_args->node = node[id];
                    node_frame_args->media = ra_packet_scheduler_args->media;
                    node_frame_args->cnt_media = ra_packet_scheduler_args->cnt_media;
                    node_frame_args->media_id = media_id;

                    /* activate node heartbeat checker & sender */
                    pthread_t heartbeat_checker, heartbeat_sender;
                    pthread_create(&heartbeat_checker, NULL, ra_check_heartbeat, node[id]);
                    pthread_create(&heartbeat_sender, NULL, ra_send_heartbeat, node[id]);

                    *ra_packet_scheduler_args->cnt_remote_sock =
                            (*ra_packet_scheduler_args->cnt_remote_sock) + 1;
                    *ra_packet_scheduler_args->cnt_node = (*ra_packet_scheduler_args->cnt_node) + 1;

                    pthread_create(&frame_sender, NULL, ra_node_frame_sender, node_frame_args);
                    if(frame_receiver == NULL) {
                        frame_receiver = malloc(sizeof(pthread_t));
                        pthread_create(frame_receiver, NULL, ra_node_frame_receiver, node_frame_args);
                    }
                } else {
                    enqueue_task(node[node_id]->recv_queue, task);
                }
            }
        }
    }
}
