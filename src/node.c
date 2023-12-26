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

#include "raplayer/node.h"

void *ra_check_heartbeat(void *p_heartbeat_checker_args) {
    ra_node_t *node = p_heartbeat_checker_args;
    struct timespec timespec;
    timespec.tv_sec = 1;
    timespec.tv_nsec = 0;

    while (true) {
        if (!(node->status & RA_NODE_CONNECTION_EXHAUSTED)) {
            node->status &= ~RA_NODE_HEARTBEAT_RECEIVED;
            nanosleep(&timespec, NULL);

            if (!(node->status & RA_NODE_HEARTBEAT_RECEIVED)) {
                RA_WARN("%02llu: Connection closed with %s:%d\n",
                        node->id + 1, inet_ntoa(node->remote_sock->addr.sin_addr),
                        ntohs(node->remote_sock->addr.sin_port));
                RA_WARN("Receiving client heartbeat timed out.\n");
                RA_WARN("Stopped Sending Opus Packets and cleaned up.\n");
                fflush(stdout);
                node->status = RA_NODE_CONNECTION_EXHAUSTED;
                break;
            }
        }
    }
    return NULL;
}

void *ra_send_heartbeat(void *p_heartbeat_sender_args) {
    ra_node_t *node = p_heartbeat_sender_args;
    struct timespec timespec;
    timespec.tv_sec = 0;
    timespec.tv_nsec = 250000000;

    void *heartbeat_msg = malloc(RA_DWORD + RA_BYTE);
    memcpy(heartbeat_msg, RA_CTL_HEADER, RA_DWORD);
    memcpy(heartbeat_msg + RA_DWORD, "\x00", RA_BYTE);

    while (!(node->status & RA_NODE_CONNECTION_EXHAUSTED)) {
        sendto((int) node->local_sock->fd, heartbeat_msg, RA_DWORD + RA_BYTE, 0,
               (struct sockaddr *) &node->remote_sock->addr,
               sizeof(struct sockaddr_in));
        nanosleep(&timespec, NULL);
    }

    free(heartbeat_msg);
    return EXIT_SUCCESS;
}

void *ra_20ms_opus_builder(void *p_opus_builder_args) {
    ra_opus_builder_args_t *opus_builder_args = (ra_opus_builder_args_t *) p_opus_builder_args;
    ra_node_t *node = opus_builder_args->node;
    OpusRepacketizer *repacketizer = opus_repacketizer_create();

    ra_media_t *local_provider = NULL, **media = *opus_builder_args->media;
    uint64_t cnt_media = *opus_builder_args->cnt_media;
    int64_t *media_seq_list = calloc(cnt_media, sizeof(int64_t));
    int64_t *media_seq_adj_list = calloc(cnt_media, sizeof(int64_t));
    for (int i = 0; i < cnt_media; i++) {
        if (media[i]->src == node->local_sock) {
            bool is_local_provider = false;
            if (((is_local_provider = (media[i]->type == RA_MEDIA_TYPE_LOCAL_PROVIDE)) ||
                 media[i]->type == RA_MEDIA_TYPE_REMOTE_PROVIDE) && !(media[i]->status & RA_MEDIA_TERMINATED) &&
                media[i] != node->remote_media) {
                if (is_local_provider)
                    local_provider = media[i];
                media_seq_list[i] = -1;
                media_seq_adj_list[i] = -1;
            }
        }
    }

    if (local_provider == NULL)
        RA_DEBUG_MORE(YEL, "[node] id: %llu, local media provider not registered\n", node->id);

    ra_rtp_t rtp_header;
    ra_rtp_init_context(&rtp_header);
    while (true) {
        if (cnt_media != *opus_builder_args->cnt_media) {
            media = *opus_builder_args->media;
            cnt_media = *opus_builder_args->cnt_media;
            ra_realloc(media_seq_list, cnt_media * sizeof(int64_t));
        }

        for (int i = 0; i < cnt_media; i++) {
            if (!(media[i]->status & RA_MEDIA_TERMINATED) && media_seq_list[i] == 0 && media[i]->src == node->local_sock
                && media[i]->type == RA_MEDIA_TYPE_REMOTE_PROVIDE && media[i] != node->remote_media) {
                media_seq_list[i] = -1;
                media_seq_adj_list[i] = -1;
                RA_DEBUG_MORE(GRN, "[node] id: %llu, new remote provider detected - media id: %llu.\n", node->id,
                              media[i]->id);
            }
        }

        for (int i = 0; i < cnt_media; i++) {
            if (!(media[i]->status & RA_MEDIA_TERMINATED) && media_seq_list[i] == -1) {
                ra_task_t *frame = retrieve_task(media[i]->current.queue, -1, false);
                if (frame != NULL) {
                    media_seq_list[i] = ra_swap_endian_uint16(ra_rtp_get_context(frame->data).sequence);
                    media_seq_adj_list[i] = 0;
                    RA_DEBUG_MORE(GRN, "[node] id: %llu, media id: %llu, set rtp sequence to %llu.\n", node->id,
                                  media[i]->id, media_seq_list[i]);
                }
            }
        }

        pthread_mutex_lock(opus_builder_args->opus_builder_mutex);
        while (*opus_builder_args->turn != 0) {
            pthread_cond_wait(opus_builder_args->opus_builder_cond, opus_builder_args->opus_builder_mutex);
        }

        if (node->status & RA_NODE_CONNECTION_EXHAUSTED) {
            *opus_builder_args->turn = 1;
            pthread_cond_signal(opus_builder_args->opus_builder_cond);
            pthread_mutex_unlock(opus_builder_args->opus_builder_mutex);
            break;
        }

        media = *opus_builder_args->media;
        repacketizer = opus_repacketizer_init(repacketizer);
        for (int i = 0; i < cnt_media; i++) {
            if (media_seq_list[i] > 0) {
                for (int j = 0; j < 7; j++) {
                    ra_task_t *current_frame = retrieve_task(media[i]->current.queue, j, false);
                    if (current_frame != NULL) {
                        ra_rtp_t current_rtp_header = ra_rtp_get_context(current_frame->data);
//                        printf("[%llu] %hu, %lld\n", node->id, ra_swap_endian_uint16(current_rtp_header.sequence), media_seq_list[i]);
                        opus_int32 current_rtp_length = (opus_int32) ra_rtp_get_length(current_rtp_header);
                        if ((media_seq_list[i]) ==
                            ra_swap_endian_uint16(current_rtp_header.sequence)) {
                            opus_repacketizer_cat(repacketizer, current_frame->data + current_rtp_length,
                                                  ((opus_int32) current_frame->data_len - current_rtp_length));
                            media_seq_list[i] = (media_seq_list[i]) + 1;
                            media_seq_adj_list[i] = 0;
                            free(current_frame);
                            break;
                        } else
                            media_seq_adj_list[i] = 1;
                    }
                }

                if (media_seq_adj_list[i] == 1) {
                    media_seq_adj_list[i] = 0;
                    media_seq_list[i] = ra_swap_endian_uint16(
                            ra_rtp_get_context(retrieve_task(media[i]->current.queue, -1, false)->data).sequence);
                    media_seq_list[i] = (media_seq_list[i] == 0) ? 1 : media_seq_list[i];
                    RA_DEBUG_MORE(YEL, "[node] id: %llu, media id: %llu, adj sequence to %llu.\n", node->id,
                                  media[i]->id, media_seq_list[i]);
                }
//                printf("%d\n", repacketized_frame->data_len);
//                printf("media provider id %d seq:\n", i);
//                int64_t seq_offset = (int64_t) ((*media_seq_list[i]) - media[i]->current.sequence);
//                printf("seq_offset: %llu - %llu = %lld\n", *media_seq_list[i], media[i]->current.sequence, seq_offset);

//                if (current_frame == NULL)
//                    current_frame = retrieve_task(media[i]->current.queue, -1, false);

//                if (current_frame != NULL) {
//                    memcpy(pcm_bytes, current_frame->data, current_frame->data_len);
//                    for (int j = 0; j < current_frame->data_len / RA_WORD; j++)
//                        frame[j] = ra_mix_frame_pcm16le(frame[j], ((int16_t *) current_frame->data)[j]);
//                    *media_seq_list[i] = (*media_seq_list[i]) + 1;

                /* Enqueue the payload. */
//                    printf("seq: %d\n", ra_rtp_get_context(current_frame->data).sequence);
//                    enqueue_task(node->send_queue, current_frame);
//                }
            }
        }

        ra_task_t *repacketized_frame = create_task(RA_MAX_DATA_SIZE);
        ra_rtp_set_next(&rtp_header, 960);
        uint64_t rtp_header_len = ra_rtp_get_length(rtp_header);
        memcpy(repacketized_frame->data, rtp_header.without_csrc_data, rtp_header_len);
        repacketized_frame->data_len = rtp_header_len;

        int nbBytes = opus_repacketizer_out(repacketizer, repacketized_frame->data + rtp_header_len, RA_FRAME_SIZE);
        if (nbBytes > 0) {
            repacketized_frame->data_len += nbBytes;
            enqueue_task(node->send_queue, repacketized_frame);
        }

        *opus_builder_args->turn = 1;
        pthread_cond_signal(opus_builder_args->opus_builder_cond);
        pthread_mutex_unlock(opus_builder_args->opus_builder_mutex);
    }
    return NULL;
}

void *ra_20ms_opus_sender(void *p_opus_sender_args) {
    ra_opus_sender_args_t *opus_sender_args = (ra_opus_sender_args_t *) p_opus_sender_args;
    while (!(opus_sender_args->node->status & RA_NODE_CONNECTION_EXHAUSTED)) {
        ra_task_t *opus_frame = dequeue_task(opus_sender_args->node->send_queue);
        pthread_mutex_lock(opus_sender_args->opus_sender_mutex);
        pthread_cond_wait(opus_sender_args->opus_sender_cond, opus_sender_args->opus_sender_mutex);
        sendto((int) opus_sender_args->node->local_sock->fd, opus_frame->data,
               opus_frame->data_len, 0, (struct sockaddr *) &opus_sender_args->node->remote_sock->addr,
               sizeof(struct sockaddr_in));
        pthread_mutex_unlock(opus_sender_args->opus_sender_mutex);
        destroy_task(opus_frame);
    }
    return NULL;
}

void *ra_20ms_opus_timer(void *p_opus_timer_args) {
    ra_opus_timer_args_t *opus_timer_args = (ra_opus_timer_args_t *) p_opus_timer_args;

    struct timespec start_timespec;
    clock_gettime(CLOCK_MONOTONIC, &start_timespec);
    time_t start_time = (start_timespec.tv_sec * 1000000000L) + start_timespec.tv_nsec, time, offset = 0L, average = 0L;

    while (!(opus_timer_args->node->status & RA_NODE_CONNECTION_EXHAUSTED)) {
        offset += 20000000L;
        time = start_time + offset;

        pthread_mutex_lock(opus_timer_args->opus_builder_mutex);
        while (*opus_timer_args->turn != 1) {
            pthread_cond_wait(opus_timer_args->opus_builder_cond, opus_timer_args->opus_builder_mutex);
        }

        struct timespec current_time, calculated_delay;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        time -= ((current_time.tv_sec * 1000000000L) + current_time.tv_nsec);

        calculated_delay.tv_sec = ((time / 1000000000L) > 0 ? (time / 1000000000L) : 0);
        calculated_delay.tv_nsec = ((time % 1000000000L) > 0 ? (time % 1000000000L) : average);

        /* Calculates the time average value for when the current time exceeds the calculated time. */
        average = (average == 0L ? ((time % 1000000000L) > 0 ? (time % 1000000000L) : average)
                                 : ((time % 1000000000L) > 0 ? ((time % 1000000000L) + average) / 2 : average));

        nanosleep(&calculated_delay, NULL);
        pthread_mutex_lock(opus_timer_args->opus_sender_mutex);
        pthread_cond_broadcast(opus_timer_args->opus_sender_cond);
        pthread_mutex_unlock(opus_timer_args->opus_sender_mutex);

        *opus_timer_args->turn = 0;
        pthread_cond_signal(opus_timer_args->opus_builder_cond);
        pthread_mutex_unlock(opus_timer_args->opus_builder_mutex);

        /* Adjusts the 20ms interval if the average value was used instead. */
        if (calculated_delay.tv_nsec == average)
            average -= 250000L;
    }
    return NULL;
}

void *ra_node_frame_receiver(void *p_node_frame_args) {
    ra_node_frame_args_t *node_frame_args = ((ra_node_frame_args_t *) p_node_frame_args);
    ra_node_t *node = node_frame_args->node;
    ra_media_t **media = *node_frame_args->media;
    uint64_t cnt_media = *node_frame_args->cnt_media;

    OpusRepacketizer *repacketizer = opus_repacketizer_create();

    int64_t *media_seq_list = calloc(cnt_media, sizeof(int64_t));
    int64_t *media_seq_adj_list = calloc(cnt_media, sizeof(int64_t));
    for (int i = 0; i < cnt_media; i++) {
        if (media[i]->src == node->local_sock) {
            if (media[i]->type == RA_MEDIA_TYPE_REMOTE_PROVIDE && !(media[i]->status & RA_MEDIA_TERMINATED)) {
                media_seq_list[i] = -1;
                media_seq_adj_list[i] = -1;
            }
        }
    }

//    chacha20_init_context(&ctx, crypto_payload, crypto_payload + CHACHA20_NONCEBYTES, 0);

//    if (local_consumer == NULL) {
//        RA_DEBUG_MORE(YEL, "local consumer not registered! consumer will not active.\n");
//        return NULL;
//    }

    struct timespec start_timespec, current_time, calculated_delay;
    clock_gettime(CLOCK_MONOTONIC, &start_timespec);
    time_t start_time = (start_timespec.tv_sec * 1000000000L) + start_timespec.tv_nsec, time, offset = 0L;
    while (true) {
        if (cnt_media != *node_frame_args->cnt_media) {
            media = *node_frame_args->media;
            cnt_media = *node_frame_args->cnt_media;
            ra_realloc(media_seq_list, cnt_media * sizeof(int64_t));
        }

        for (int i = 0; i < cnt_media; i++) {
            if (!(media[i]->status & RA_MEDIA_TERMINATED) && media_seq_list[i] == 0 &&
                media[i]->src == node->local_sock && media[i]->type == RA_MEDIA_TYPE_REMOTE_PROVIDE) {
                media_seq_list[i] = -1;
                media_seq_adj_list[i] = -1;
                RA_DEBUG_MORE(GRN, "[node] id: %llu, new remote provider detected - media id: %llu.\n", node->id,
                              media[i]->id);
            }
        }

        for (int i = 0; i < cnt_media; i++) {
            if (!(media[i]->status & RA_MEDIA_TERMINATED) && media_seq_list[i] == -1) {
                ra_task_t *frame = retrieve_task(media[i]->current.queue, -1, false);
                if (frame != NULL) {
                    media_seq_list[i] = ra_swap_endian_uint16(ra_rtp_get_context(frame->data).sequence);
                    media_seq_adj_list[i] = 0;
                    RA_DEBUG_MORE(GRN, "[node] id: %llu, media id: %llu, set rtp sequence to %llu.\n", node->id,
                                  media[i]->id, media_seq_list[i]);
                }
            }
        }

        offset += 20000000L;
        time = start_time + offset;

        repacketizer = opus_repacketizer_init(repacketizer);
        for (int i = 0; i < cnt_media; i++) {
            if (media_seq_list[i] > 0) {
                for (int j = 0; j < 7; j++) {
                    ra_task_t *current_frame = retrieve_task(media[i]->current.queue, j, false);
                    if (current_frame != NULL) {
                        ra_rtp_t current_rtp_header = ra_rtp_get_context(current_frame->data);
                        opus_int32 current_rtp_length = (opus_int32) ra_rtp_get_length(current_rtp_header);
                        if ((media_seq_list[i]) ==
                            ra_swap_endian_uint16(current_rtp_header.sequence)) {
                            opus_repacketizer_cat(repacketizer, current_frame->data + current_rtp_length,
                                                  ((opus_int32) current_frame->data_len - current_rtp_length));
                            media_seq_list[i] = (media_seq_list[i]) + 1;
                            media_seq_adj_list[i] = 0;
                            free(current_frame);
                            break;
                        } else
                            media_seq_adj_list[i] = 1;
                    }
                }

                if (media_seq_adj_list[i] == 1) {
                    media_seq_adj_list[i] = 0;
                    media_seq_list[i] = ra_swap_endian_uint16(
                            ra_rtp_get_context(retrieve_task(media[i]->current.queue, -1, false)->data).sequence);
                    media_seq_list[i] = (media_seq_list[i] == 0) ? 1 : media_seq_list[i];
                    RA_DEBUG_MORE(YEL, "[node] id: %llu, media id: %llu, adj sequence to %llu.\n", node->id,
                                  media[i]->id, media_seq_list[i]);
                }
            }
        }

        int nbCnt = opus_repacketizer_get_nb_frames(repacketizer);
        if (nbCnt > 0) {
            ra_task_t *repacketized_frame = create_task(RA_FRAME_SIZE);
            repacketized_frame->data_len = opus_repacketizer_out(repacketizer, repacketized_frame->data, RA_FRAME_SIZE);
            for (int i = 0; i < cnt_media; i++) {
                if (!(media[i]->status & RA_MEDIA_TERMINATED) &&
                    media[i]->type == RA_MEDIA_TYPE_LOCAL_CONSUME && media[i]->src == node->local_sock) {
                    enqueue_task(media[i]->current.queue, repacketized_frame);
                }
            }
        }

        clock_gettime(CLOCK_MONOTONIC, &current_time);
        time -= ((current_time.tv_sec * 1000000000L) + current_time.tv_nsec);

        calculated_delay.tv_sec = ((time / 1000000000L) > 0 ? (time / 1000000000L) : 0);
        calculated_delay.tv_nsec = ((time % 1000000000L) > 0 ? (time % 1000000000L) : 0);
        nanosleep(&calculated_delay, NULL);
    }
    return 0;
}

void *ra_node_frame_sender(void *p_node_frame_args) {
    ra_node_t *node = ((ra_node_frame_args_t *) p_node_frame_args)->node;

    pthread_t opus_timer;
    pthread_t opus_builder;
    pthread_t opus_sender;

    pthread_mutex_t opus_builder_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t opus_sender_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t opus_builder_cond = PTHREAD_COND_INITIALIZER;
    pthread_cond_t opus_sender_cond = PTHREAD_COND_INITIALIZER;

    /* Construct opus builder, sender context. */
    ra_opus_builder_args_t *p_opus_builder_args = malloc(sizeof(ra_opus_builder_args_t));
    p_opus_builder_args->node = node;
    p_opus_builder_args->media = ((ra_node_frame_args_t *) p_node_frame_args)->media;
    p_opus_builder_args->cnt_media = ((ra_node_frame_args_t *) p_node_frame_args)->cnt_media;
    p_opus_builder_args->turn = malloc(sizeof(int));
    *p_opus_builder_args->turn = 0;
    p_opus_builder_args->opus_builder_mutex = &opus_builder_mutex;
    p_opus_builder_args->opus_builder_cond = &opus_builder_cond;

    ra_opus_timer_args_t *p_opus_timer_args = malloc(sizeof(ra_opus_timer_args_t));
    p_opus_timer_args->node = node;
    p_opus_timer_args->turn = p_opus_builder_args->turn;
    p_opus_timer_args->opus_builder_mutex = &opus_builder_mutex;
    p_opus_timer_args->opus_builder_cond = &opus_builder_cond;
    p_opus_timer_args->opus_sender_mutex = &opus_sender_mutex;
    p_opus_timer_args->opus_sender_cond = &opus_sender_cond;

    ra_opus_sender_args_t *p_opus_sender_args = malloc(sizeof(ra_opus_sender_args_t));
    p_opus_sender_args->node = node;
    p_opus_sender_args->opus_sender_mutex = &opus_sender_mutex;
    p_opus_sender_args->opus_sender_cond = &opus_sender_cond;

    /* activate opus builder, timer, sender */
    pthread_create(&opus_builder, NULL, ra_20ms_opus_builder, (void *) p_opus_builder_args);
    pthread_create(&opus_timer, NULL, ra_20ms_opus_timer, (void *) p_opus_timer_args);
    pthread_create(&opus_sender, NULL, ra_20ms_opus_sender, (void *) p_opus_sender_args);

    /* Wait for exiting threads. */
    int ret;
    pthread_join(opus_builder, (void **) &ret);
    pthread_join(opus_timer, NULL);
    pthread_join(opus_sender, NULL);

    /* Send EOS to clients && clean up. */
//    for (int i = 0; i < client_count; i++) {
//        ra_node_t *client = &(*client_context)[i];
//        sendto(task_scheduler_args.sock_fd, EOS, strlen(EOS), 0, (const struct sockaddr *) &client->node_addr,
//               client->socket_len);
//    }
//    free(*client_context);
//    free(client_context);
    return (void *) (uintptr_t) ret;
}