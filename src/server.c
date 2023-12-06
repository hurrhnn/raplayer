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
#include <raplayer/client.h>
#include <raplayer/server.h>
#include <raplayer/chacha20.h>
#include <raplayer/task_scheduler.h>
#include <raplayer/task_queue.h>
#include <raplayer/utils.h>

int server_init_socket(ra_server_t *server, uint64_t idx) {
    /* Create socket file descriptor */
    if ((server->list[idx].sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        return RAPLAYER_SOCKET_CREATION_FAILED;

    server->list[idx].server_addr.sin_family = AF_INET; // IPv4
    server->list[idx].server_addr.sin_port = htons((uint16_t) server->list[idx].port);
    server->list[idx].server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Bind the socket with the server address. */
    if((bind(server->list[idx].sock_fd, (struct sockaddr *) &server->list[idx].server_addr,
             sizeof(struct sockaddr))) < 0)
        return RA_SERVER_SOCKET_BIND_FAILED;

    return 0;
}

int ready_sock_server_seq1(ra_task_queue_t *recv_queue) {
    ra_task_t *task = retrieve_task(recv_queue);
    bool is_verified = (strncmp((char *) task->data, "HELLO", task->data_len) == 0);
    remove_task(task);

    return (is_verified ? 0 : RA_SERVER_SOCKET_INIT_SEQ1_FAILED);
}

int ready_sock_server_seq2(ra_node_t *client, uint32_t len) {
    bool is_verified = false;
    int stream_info_size = WORD + DWORD + WORD;
    int buffer_size = 5;

    uint16_t channels = OPUS_AUDIO_CH;
    uint32_t sample_rate = OPUS_AUDIO_SR;
    uint16_t bits_per_sample = OPUS_AUDIO_BPS;

    char stream_info[stream_info_size];
    memcpy(stream_info, &channels, WORD);
    memcpy((stream_info + WORD), &sample_rate, DWORD);
    memcpy((stream_info + WORD + DWORD), &bits_per_sample, WORD);

    char *buffer = calloc(buffer_size, BYTE);
    sprintf(buffer, "%d", stream_info_size);

    sendto(client->sock_fd, buffer, buffer_size, 0,
           (struct sockaddr *) &client->node_addr,
           client->socket_len);
    free(buffer);

    ra_task_t *task = retrieve_task(client->recv_queue);
    if (strncmp((char *) task->data, "OK", task->data_len) != 0) {
        remove_task(task);
        return RA_SERVER_SOCKET_INIT_SEQ2_FAILED;
    }

    if (stream_info_size ==
        sendto(client->sock_fd, stream_info, stream_info_size, 0,
               (struct sockaddr *) &client->node_addr, client->socket_len)) {

        task = retrieve_task(client->recv_queue);
        if (stream_info_size == strtol((char *) task->data, NULL, 10)) {
            sendto(client->sock_fd, &len, DWORD, 0, (struct sockaddr *) &client->node_addr, client->socket_len);

            remove_task(task);
            task = retrieve_task(client->recv_queue);
            if (strncmp((char *) task->data, "OK", task->data_len) == 0) {
                is_verified = true;
                remove_task(task);
            }
        }
    }
    return (is_verified ? 0 : RA_SERVER_SOCKET_INIT_SEQ2_FAILED);
}

int ready_sock_server_seq3(ra_node_t *client) {
    bool is_verified = false;
    const int crypto_payload_size = CHACHA20_NONCEBYTES + CHACHA20_KEYBYTES;
    unsigned char *crypto_payload = generate_random_bytestream(crypto_payload_size);

    if (crypto_payload_size ==
        sendto(client->sock_fd, crypto_payload, crypto_payload_size, 0,
               (struct sockaddr *) &client->node_addr, client->socket_len)) {

        ra_task_t *task = retrieve_task(client->recv_queue);
        if (!strncmp((char *) task->data, OK, task->data_len)) {
            memset(&(*client).crypto_context, 0x0, sizeof(struct chacha20_context));
            chacha20_init_context(&(*client).crypto_context, crypto_payload, crypto_payload + CHACHA20_NONCEBYTES, 0);
            remove_task(task);
            is_verified = true;
        }
    }
    free(crypto_payload);
    return is_verified ? 0 : RA_SERVER_SOCKET_INIT_SEQ3_FAILED;
}

void *provide_20ms_opus_builder(void *p_opus_builder_args) {
    opus_builder_args_t *opus_builder_args = (opus_builder_args_t *) p_opus_builder_args;

    unsigned char *buffer = calloc(MAX_DATA_SIZE, BYTE);
    opus_int16 in[FRAME_SIZE * OPUS_AUDIO_CH];
    unsigned char c_bits[FRAME_SIZE]; // TODO: need to change for correspond adaptive latency

    while (true) {
        pthread_mutex_lock(opus_builder_args->opus_builder_mutex);
        while (*opus_builder_args->turn != 0) {
            pthread_cond_wait(opus_builder_args->opus_builder_cond, opus_builder_args->opus_builder_mutex);
        }
        /* Read 16bit/sample audio frame. */
        unsigned char *pcm_bytes = opus_builder_args->frame_callback(opus_builder_args->callback_user_data);

        if (!*opus_builder_args->is_sender_ready) {
            *opus_builder_args->turn = 1;
            pthread_cond_signal(opus_builder_args->opus_builder_cond);
            pthread_mutex_unlock(opus_builder_args->opus_builder_mutex);
            continue;
        }

        /* Convert from little-endian ordering. */
        for (int i = 0; i < OPUS_AUDIO_CH * FRAME_SIZE; i++)
            in[i] = (opus_int16) (pcm_bytes[2 * i + 1] << 8 | pcm_bytes[2 * i]);

        /* Encode the frame. */
        int nbBytes = opus_encode(opus_builder_args->encoder, in, FRAME_SIZE, c_bits, FRAME_SIZE);
        if (nbBytes < 0) {
            printf("Error: opus encode failed - %s\n", opus_strerror(nbBytes));
            *opus_builder_args->status = RA_NODE_CONNECTION_EXHAUSTED;
            break;
        }

        /* Create payload. */
        const unsigned int nbBytes_len = (int) floor(log10(nbBytes) + 1);
        sprintf((char *) buffer, "%d", nbBytes);
        strcat((char *) buffer, OPUS_FLAG);
        memcpy(buffer + nbBytes_len + sizeof(OPUS_FLAG) - 1, c_bits, nbBytes);

        /* Encrypt the frame. */
        // chacha20_xor(&crypto_context, c_bits, nbBytes); /* use client's crypto context instead of shared context */
        for (int i = 0; i < *opus_builder_args->client_count; i++) {
            pthread_rwlock_rdlock(opus_builder_args->client_context_rwlock);
            ra_node_t *client = &(*opus_builder_args->client_context)[i];
            if ((client->status & RA_NODE_INITIATED) && (client->status & RA_NODE_CONNECTED)) {
                ra_task_t *opus_frame = create_task(MAX_DATA_SIZE);
                memset(opus_frame->data, 0x0, opus_frame->data_len);
                opus_frame->data_len = (ssize_t) (nbBytes_len + sizeof(OPUS_FLAG) - 1 + nbBytes);
                memcpy(opus_frame->data, buffer, opus_frame->data_len);
                chacha20_xor(&client->crypto_context, opus_frame->data + nbBytes_len + sizeof(OPUS_FLAG) - 1,
                             nbBytes);

                /* Enqueue the payload. */
                append_task(client->send_queue, opus_frame);
            }
            pthread_rwlock_unlock(opus_builder_args->client_context_rwlock);
        }
        *opus_builder_args->turn = 1;
        pthread_cond_signal(opus_builder_args->opus_builder_cond);
        pthread_mutex_unlock(opus_builder_args->opus_builder_mutex);
    }
    *opus_builder_args->status = RA_NODE_CONNECTION_EXHAUSTED;
    free(buffer);
    return NULL;
}

void *provide_20ms_opus_sender(void *p_opus_sender_args) {
    opus_sender_args_t *opus_sender_args = (opus_sender_args_t *) p_opus_sender_args;
    while ((!*opus_sender_args->status)) {
        pthread_rwlock_rdlock(opus_sender_args->client_context_rwlock);
        ra_node_t *client = &(*opus_sender_args->client_context)[opus_sender_args->client_id];
        if ((client->status & RA_NODE_INITIATED) && (client->status & RA_NODE_CONNECTED)) {
            ra_task_t *opus_frame = retrieve_task(client->send_queue);
            pthread_mutex_lock(opus_sender_args->opus_sender_mutex);
            pthread_cond_wait(opus_sender_args->opus_sender_cond, opus_sender_args->opus_sender_mutex);
            sendto(client->sock_fd, opus_frame->data,
                   opus_frame->data_len, 0, (struct sockaddr *) &client->node_addr, client->socket_len);
            pthread_mutex_unlock(opus_sender_args->opus_sender_mutex);
            pthread_rwlock_unlock(opus_sender_args->client_context_rwlock);
            free(opus_frame);
        } else {
            pthread_rwlock_unlock(opus_sender_args->client_context_rwlock);
            break;
        }
    }
    return NULL;
}

void *provide_20ms_opus_timer(void *p_opus_timer_args) {
    opus_timer_args_t *opus_timer_args = (opus_timer_args_t *) p_opus_timer_args;

    struct timespec start_timespec;
    clock_gettime(CLOCK_MONOTONIC, &start_timespec);
    time_t start_time = (start_timespec.tv_sec * 1000000000L) + start_timespec.tv_nsec, time, offset = 0L, average = 0L;

    while (!(*opus_timer_args->status)) {
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

void *check_heartbeat(void *p_heartbeat_receiver_args) {
    struct timespec timespec;
    timespec.tv_sec = 1;
    timespec.tv_nsec = 0;

    while (true) {
        pthread_rwlock_rdlock(((void **) p_heartbeat_receiver_args)[2]);
        ra_node_t *client = &(*((ra_node_t **) ((void **) p_heartbeat_receiver_args)[0]))
        [((opus_sender_args_t *) ((void **) p_heartbeat_receiver_args)[1])->client_id];

        if ((client->status & RA_NODE_INITIATED) && (client->status & RA_NODE_CONNECTED)) {
            client->status &= ~RA_NODE_HEARTBEAT_RECEIVED;
            pthread_rwlock_unlock(((void **) p_heartbeat_receiver_args)[2]);
            nanosleep(&timespec, NULL);

            pthread_rwlock_rdlock(((void **) p_heartbeat_receiver_args)[2]);
            client = &(*((ra_node_t **) ((void **) p_heartbeat_receiver_args)[0]))
            [((opus_sender_args_t *) ((void **) p_heartbeat_receiver_args)[1])->client_id];

            if (!(client->status & RA_NODE_HEARTBEAT_RECEIVED)) {
                printf("\n%02llu: Connection closed by %s:%d",
                       client->node_id + 1, inet_ntoa(client->node_addr.sin_addr),
                       ntohs(client->node_addr.sin_port));
                printf("\nReceiving client heartbeat timed out.\nStopped Sending Opus Packets and cleaned up.\n");
                fflush(stdout);
                client->status = RA_NODE_CONNECTION_EXHAUSTED;
                pthread_rwlock_unlock(((void **) p_heartbeat_receiver_args)[2]);
                break;
            }
        }
        pthread_rwlock_unlock(((void **) p_heartbeat_receiver_args)[2]);
    }
    return NULL;
}

_Noreturn void *handle_client(void *p_client_handler_args) {
    client_handler_args_t *client_handler_args = p_client_handler_args;
    const int *client_count = client_handler_args->client_count;

    pthread_mutex_t *complete_init_queue_mutex = client_handler_args->complete_init_mutex[0];
    pthread_cond_t *complete_init_queue_cond = client_handler_args->complete_init_cond[0];
    pthread_mutex_t *complete_init_client_mutex = client_handler_args->complete_init_mutex[1];
    pthread_cond_t *complete_init_client_cond = client_handler_args->complete_init_cond[1];

    while (true) {
        pthread_mutex_lock(complete_init_queue_mutex);
        pthread_cond_wait(complete_init_queue_cond, complete_init_queue_mutex);
        pthread_mutex_unlock(complete_init_queue_mutex);

        ra_node_t *client = &(*client_handler_args->client_context)[(*client_count) - 1];
        if (!ready_sock_server_seq1(client->recv_queue)) {
            if (!ready_sock_server_seq2(client, client_handler_args->data_len)) {
                if (!ready_sock_server_seq3(client)) {
                    client->status |= RA_NODE_CONNECTED;
                    printf("Preparing socket sequence has been Successfully Completed.");
                }
                else {
                    printf("Error: A crypto preparation sequence Failed.\n");
                    continue;
                }
            } else {
                printf("Error: A server socket preparation sequence Failed.\n");
                continue;
            }
        } else {
            printf("Error: A client socket preparation sequence Failed.\n");
            continue;
        }

        printf("\nStarted Sending Opus Packets...\n");
        fflush(stdout);

        pthread_mutex_lock(complete_init_client_mutex);
        pthread_cond_broadcast(complete_init_client_cond);
        pthread_mutex_unlock(complete_init_client_mutex);

        pthread_t opus_sender;
        pthread_t heartbeat_checker;

        opus_sender_args_t *p_opus_sender_args = malloc(sizeof(opus_sender_args_t));
        p_opus_sender_args->status = client_handler_args->status;
        p_opus_sender_args->client_id = (*client_count) - 1;
        p_opus_sender_args->client_context = client_handler_args->client_context;
        p_opus_sender_args->opus_sender_mutex = client_handler_args->opus_sender_mutex;
        p_opus_sender_args->opus_sender_cond = client_handler_args->opus_sender_cond;
        p_opus_sender_args->client_context_rwlock = client_handler_args->client_context_rwlock;

        void **heartbeat_checker_args = calloc(sizeof(void *), WORD + BYTE);
        heartbeat_checker_args[0] = client_handler_args->client_context;
        heartbeat_checker_args[1] = p_opus_sender_args;
        heartbeat_checker_args[2] = client_handler_args->client_context_rwlock;

        // Activate opus sender.
        pthread_create(&opus_sender, NULL, provide_20ms_opus_sender, (void *) p_opus_sender_args);
        pthread_create(&heartbeat_checker, NULL, check_heartbeat, (void *) heartbeat_checker_args);

        client_handler_args->is_sender_ready = true;
    }
}

void *ra_server(void *p_server) {
    /* Store ra_server idx. */
    ra_server_t *server = p_server;
    uint64_t idx = server->idx;

    /* Create a new encoder state. */
    int err;
    OpusEncoder *encoder;
    encoder = opus_encoder_create((opus_int32) OPUS_AUDIO_SR, OPUS_AUDIO_CH,
                                  OPUS_APPLICATION,
                                  &err);
    if (err < 0) {
        printf("Error: failed to create an encoder - %s\n", opus_strerror(err));
        return (void *) RA_SERVER_CREATE_OPUS_ENCODER_FAILED;
    }

    if (opus_encoder_ctl(encoder,
                         OPUS_SET_BITRATE(OPUS_AUDIO_SR * OPUS_AUDIO_CH)) <
        0) {
        printf("Error: failed to set bitrate - %s\n", opus_strerror(err));
        return (void *) RA_SERVER_OPUS_ENCODER_CTL_FAILED;
    }

    err = server_init_socket(server, idx);
    if(err == -1)
        return (void *) (uintptr_t) err;

    int client_count = 0;
    pthread_t opus_timer;
    pthread_t opus_builder;

    pthread_mutex_t opus_builder_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t opus_sender_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t complete_init_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t complete_init_client_mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_cond_t opus_builder_cond = PTHREAD_COND_INITIALIZER;
    pthread_cond_t opus_sender_cond = PTHREAD_COND_INITIALIZER;
    pthread_cond_t complete_init_queue_cond = PTHREAD_COND_INITIALIZER;
    pthread_cond_t complete_init_client_cond = PTHREAD_COND_INITIALIZER;

    pthread_rwlock_t client_context_rwlock = PTHREAD_RWLOCK_INITIALIZER;

    ra_node_t **client_context = malloc(sizeof(ra_node_t *));
    *client_context = NULL;

    task_scheduler_info_t task_scheduler_args;
    client_handler_args_t client_handler_args;

    task_scheduler_args.sock_fd = server->list[idx].sock_fd;
    task_scheduler_args.client_count = &client_count;
    task_scheduler_args.client_context = client_context;
    task_scheduler_args.complete_init_queue_mutex = &complete_init_queue_mutex;
    task_scheduler_args.complete_init_queue_cond = &complete_init_queue_cond;
    task_scheduler_args.client_context_rwlock = &client_context_rwlock;

    client_handler_args.status = &server->list[idx].status;
    client_handler_args.data_len = 0;
    client_handler_args.is_sender_ready = 0;
    client_handler_args.client_context = client_context;
    client_handler_args.client_count = &client_count;
    client_handler_args.complete_init_mutex[0] = &complete_init_queue_mutex;
    client_handler_args.complete_init_cond[0] = &complete_init_queue_cond;
    client_handler_args.complete_init_mutex[1] = &complete_init_client_mutex;
    client_handler_args.complete_init_cond[1] = &complete_init_client_cond;
    client_handler_args.opus_sender_mutex = &opus_sender_mutex;
    client_handler_args.opus_sender_cond = &opus_sender_cond;
    client_handler_args.client_context_rwlock = &client_context_rwlock;

    pthread_t task_scheduler;
    pthread_attr_t task_scheduler_attr;
    pthread_attr_init(&task_scheduler_attr);
    pthread_attr_setdetachstate(&task_scheduler_attr, PTHREAD_CREATE_DETACHED);

    pthread_t client_handler;
    pthread_attr_t client_handler_attr;
    pthread_attr_init(&client_handler_attr);
    pthread_attr_setdetachstate(&client_handler_attr, PTHREAD_CREATE_DETACHED);

    pthread_create(&task_scheduler, &task_scheduler_attr, schedule_task, &task_scheduler_args);
    pthread_create(&client_handler, &client_handler_attr, handle_client, &client_handler_args);

    /* Construct opus builder, sender context. */
    opus_builder_args_t *p_opus_builder_args = malloc(sizeof(opus_builder_args_t));

    p_opus_builder_args->status = &server->list[idx].status;
    p_opus_builder_args->is_sender_ready = &client_handler_args.is_sender_ready;
    p_opus_builder_args->is_stream_mode = true;
    p_opus_builder_args->frame_callback = server->list[idx].frame_callback;
    p_opus_builder_args->callback_user_data = server->list[idx].callback_user_data;
    p_opus_builder_args->turn = malloc(sizeof(int));
    *p_opus_builder_args->turn = 0;
    p_opus_builder_args->encoder = encoder;
    p_opus_builder_args->client_context = client_context;
    p_opus_builder_args->client_count = &client_count;
    p_opus_builder_args->opus_builder_mutex = &opus_builder_mutex;
    p_opus_builder_args->opus_builder_cond = &opus_builder_cond;
    p_opus_builder_args->complete_init_client_mutex = &complete_init_client_mutex;
    p_opus_builder_args->complete_init_client_cond = &complete_init_client_cond;
    p_opus_builder_args->client_context_rwlock = &client_context_rwlock;

    opus_timer_args_t *p_opus_timer_args = malloc(sizeof(opus_timer_args_t));
    p_opus_timer_args->status = &server->list[idx].status;
    p_opus_timer_args->turn = p_opus_builder_args->turn;
    p_opus_timer_args->is_stream_mode = p_opus_builder_args->is_stream_mode;
    p_opus_timer_args->opus_builder_mutex = &opus_builder_mutex;
    p_opus_timer_args->opus_builder_cond = &opus_builder_cond;
    p_opus_timer_args->opus_sender_mutex = &opus_sender_mutex;
    p_opus_timer_args->opus_sender_cond = &opus_sender_cond;
    p_opus_timer_args->complete_init_client_mutex = &complete_init_client_mutex;
    p_opus_timer_args->complete_init_client_cond = &complete_init_client_cond;

    // activate opus timer & builder.
    pthread_create(&opus_builder, NULL, provide_20ms_opus_builder, (void *) p_opus_builder_args);
    pthread_create(&opus_timer, NULL, provide_20ms_opus_timer, (void *) p_opus_timer_args);

    /* Wait for exiting threads. */
    pthread_join(opus_builder, NULL);
    pthread_join(opus_timer, NULL);

    /* Cancel unending threads. */
    pthread_cancel(task_scheduler);

    /* Send EOS to clients && clean up. */
    for (int i = 0; i < client_count; i++) {
        ra_node_t *client = &(*client_context)[i];
        sendto(task_scheduler_args.sock_fd, EOS, strlen(EOS), 0, (const struct sockaddr *) &client->node_addr,
               client->socket_len);
    }
    free(*client_context);
    free(client_context);

    /* Destroy the encoder state. */
    opus_encoder_destroy(encoder);

    return EXIT_SUCCESS;
}
