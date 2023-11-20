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
#include <raplayer/task_dispatcher.h>
#include <raplayer/utils.h>

void *consume_until_connection(void *p_stream_consumer_args) {
    void **stream_consumer_args = p_stream_consumer_args;
    unsigned char unused_buffer[WORD * WORD * FRAME_SIZE];

    while (!(*(bool *) (stream_consumer_args[1]))) {
        pthread_mutex_lock((pthread_mutex_t *) (stream_consumer_args[2]));
        pthread_cond_wait((pthread_cond_t *) (stream_consumer_args[3]), (pthread_mutex_t *) (stream_consumer_args[2]));
        pthread_mutex_unlock((pthread_mutex_t *) (stream_consumer_args[2]));

        fread(&unused_buffer, DWORD, FRAME_SIZE, stream_consumer_args[0]);
    }
    free(stream_consumer_args);
    return NULL;
}

int server_init_socket(const struct sockaddr_in *p_server_addr, int port) {
    struct sockaddr_in server_addr = *p_server_addr;
    int sock_fd;

    // Creating socket file descriptor.
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        printf("Error: Socket Creation Failed.\n");
        exit(EXIT_FAILURE);
    }

    memset((char *) &server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_port = htons((uint16_t) port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /* Bind the socket with the server address. */
    if (bind(sock_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        printf("Error: Socket Bind Failed.\n");
        exit(EXIT_FAILURE);
    }

    return sock_fd;
}

bool ready_sock_server_seq1(TaskQueue *recv_queue) {
    Task task = recvfrom_queue(recv_queue);
    if (strncmp((char *) task.buffer, "HELLO", task.buffer_len) == 0)
        return true;
    else
        return false;
}

bool ready_sock_server_seq2(TaskQueue *recv_queue, uint32_t len) {
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
    sendto(recv_queue->queue_info->sock_fd, buffer, buffer_size, 0,
           (struct sockaddr *) &recv_queue->queue_info->client->client_addr,
           recv_queue->queue_info->client->socket_len);
    free(buffer);

    Task task = recvfrom_queue(recv_queue);
    if (strncmp((char *) task.buffer, "OK", task.buffer_len) != 0) {
        return 0;
    }

    if (stream_info_size ==
        sendto(recv_queue->queue_info->sock_fd, stream_info, stream_info_size, 0,
               (struct sockaddr *) &recv_queue->queue_info->client->client_addr,
               recv_queue->queue_info->client->socket_len)) {
        task = recvfrom_queue(recv_queue);
        if (stream_info_size == strtol((char *) task.buffer, NULL, 10)) {
            sendto(recv_queue->queue_info->sock_fd, &len, DWORD, 0,
                   (struct sockaddr *) &recv_queue->queue_info->client->client_addr,
                   recv_queue->queue_info->client->socket_len);

            task = recvfrom_queue(recv_queue);
            if (strncmp((char *) task.buffer, "OK", task.buffer_len) == 0)
                return true;
        }
    }
    return false;
}

bool
ready_sock_server_seq3(TaskQueue *recv_queue) {
    const int crypto_payload_size = CHACHA20_NONCEBYTES + CHACHA20_KEYBYTES;
    unsigned char *crypto_payload = generate_random_bytestream(crypto_payload_size);

    if (crypto_payload_size ==
        sendto(recv_queue->queue_info->sock_fd, crypto_payload, crypto_payload_size, 0,
               (struct sockaddr *) &recv_queue->queue_info->client->client_addr,
               recv_queue->queue_info->client->socket_len)) {

        Task task = recvfrom_queue(recv_queue);
        if (!strncmp((char *) task.buffer, OK, task.buffer_len)) {
            memset(&recv_queue->queue_info->client->crypto_context, 0x0, sizeof(struct chacha20_context));
            chacha20_init_context(&recv_queue->queue_info->client->crypto_context, crypto_payload,
                                  crypto_payload + CHACHA20_NONCEBYTES, 0);
            return true;
        }
    }
    return false;
}

void *provide_20ms_opus_builder(void *p_opus_builder_args) {
    struct opus_builder_args *opus_builder_args = (struct opus_builder_args *) p_opus_builder_args;

    unsigned char *buffer = calloc(MAX_DATA_SIZE, BYTE);
    opus_int16 in[FRAME_SIZE * OPUS_AUDIO_CH];
    unsigned char c_bits[FRAME_SIZE]; // TODO: need to change for correspond to adaptive latency

    while (1) {
        unsigned char pcm_bytes[FRAME_SIZE * OPUS_AUDIO_CH * WORD];

        /* Read a 16 bits/sample audio frame. */
        fread(pcm_bytes, WORD * OPUS_AUDIO_CH, FRAME_SIZE,opus_builder_args->fin);
        if (feof(opus_builder_args->fin)) // End Of Stream.
            break;

        /* Convert from little-endian ordering. */
        for (int i = 0; i < OPUS_AUDIO_CH * FRAME_SIZE; i++)
            in[i] = (opus_int16) (pcm_bytes[2 * i + 1] << 8 | pcm_bytes[2 * i]);

        /* Encode the frame. */
        int nbBytes = opus_encode(opus_builder_args->encoder, in, FRAME_SIZE, c_bits, FRAME_SIZE);
        if (nbBytes < 0) {
            printf("Error: opus encode failed - %s\n", opus_strerror(nbBytes));
            exit(EXIT_FAILURE);
        }

        /* Encrypt the frame. */
//        chacha20_xor(&crypto_context, c_bits, nbBytes); /* use crypto session instead of shared context */

        /* Create payload. */
        const unsigned int nbBytes_len = (int) floor(log10(nbBytes) + 1);

        sprintf((char *) buffer, "%d", nbBytes);
        strcat((char *) buffer, OPUS_FLAG);
        memcpy(buffer + nbBytes_len + sizeof(OPUS_FLAG) - 1, c_bits, nbBytes);

        /* Waiting for opus timer's signal & Send audio frames. */
        pthread_mutex_lock(opus_builder_args->opus_builder_mutex);
        pthread_cond_wait(opus_builder_args->opus_builder_cond, opus_builder_args->opus_builder_mutex);
        pthread_mutex_lock(opus_builder_args->opus_sender_mutex);

        memcpy(opus_builder_args->opus_frame->buffer, buffer, nbBytes_len + sizeof(OPUS_FLAG) + nbBytes);
        opus_builder_args->opus_frame->buffer_len = (ssize_t) (nbBytes_len + sizeof(OPUS_FLAG) + nbBytes);

        pthread_mutex_unlock(opus_builder_args->opus_sender_mutex);
        pthread_cond_broadcast(opus_builder_args->opus_sender_cond);
        pthread_mutex_unlock(opus_builder_args->opus_builder_mutex);
    }
    **opus_builder_args->server_status = true;
    free(buffer);
    return NULL;
}

void *provide_20ms_opus_sender(void *p_opus_sender_args) {
    unsigned char **calculated_c_bits = malloc(sizeof(void *));
    struct opus_sender_args *opus_sender_args = (struct opus_sender_args *) p_opus_sender_args;

    while ((!**opus_sender_args->server_status) && opus_sender_args->recv_queue->queue_info->heartbeat_status != -1) {
        pthread_mutex_lock(opus_sender_args->opus_sender_mutex);
        pthread_cond_wait(opus_sender_args->opus_sender_cond, opus_sender_args->opus_sender_mutex);
        pthread_mutex_unlock(opus_sender_args->opus_sender_mutex);
        uint64_t nbBytes = provide_20ms_opus_offset_calculator(opus_sender_args->opus_frame->buffer, calculated_c_bits);
        chacha20_xor(&opus_sender_args->recv_queue->queue_info->client->crypto_context, *calculated_c_bits, nbBytes);
        sendto(opus_sender_args->recv_queue->queue_info->sock_fd, opus_sender_args->opus_frame->buffer,
               opus_sender_args->opus_frame->buffer_len, 0,
               (struct sockaddr *) &opus_sender_args->recv_queue->queue_info->client->client_addr,
               opus_sender_args->recv_queue->queue_info->client->socket_len);
    }
    free(calculated_c_bits);
    return NULL;
}

void *provide_20ms_opus_timer(void *p_opus_timer_args) {
    int **server_status = ((void **) p_opus_timer_args)[0];
    pthread_cond_t *p_opus_builder_cond = ((void **) p_opus_timer_args)[1];

    struct timespec start_timespec;
    clock_gettime(CLOCK_MONOTONIC, &start_timespec);
    time_t start_time = (start_timespec.tv_sec * 1000000000L) + start_timespec.tv_nsec, time, offset = 0L, average = 0L;

    while (!**server_status) {
        offset += 20000000L;
        time = start_time + offset;

        struct timespec current_time, calculated_delay;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        time -= ((current_time.tv_sec * 1000000000L) + current_time.tv_nsec);

        calculated_delay.tv_sec = ((time / 1000000000L) > 0 ? (time / 1000000000L) : 0);
        calculated_delay.tv_nsec = ((time % 1000000000L) > 0 ? (time % 1000000000L) : average);

        /* Calculates the time average value for when the current time exceeds the calculated time. */
        average = (average == 0L ? ((time % 1000000000L) > 0 ? (time % 1000000000L) : average)
                                 : ((time % 1000000000L) > 0 ? ((time % 1000000000L) + average) / 2 : average));

        nanosleep(&calculated_delay, NULL);
        pthread_cond_signal((pthread_cond_t *) p_opus_builder_cond);

        /* Adjusts the 20ms interval if the average value was used instead. */
        if (calculated_delay.tv_nsec == average)
            average -= 250000L;
    }
    return NULL;
}

void *check_heartbeat(void *p_heartbeat_receiver_args) {
    TaskQueue *recv_queue = (TaskQueue *) p_heartbeat_receiver_args;

    struct timespec timespec;
    timespec.tv_sec = 1;
    timespec.tv_nsec = 0;

    while (recv_queue->queue_info->heartbeat_status != -1) {
        recv_queue->queue_info->heartbeat_status = false;
        nanosleep(&timespec, NULL);
        if (!recv_queue->queue_info->heartbeat_status) {
            printf("\n%02d: Connection closed by %s:%d",
                   recv_queue->queue_info->client->client_id,
                   inet_ntoa(recv_queue->queue_info->client->client_addr.sin_addr),
                   ntohs(recv_queue->queue_info->client->client_addr.sin_port));
            printf("\nReceiving client heartbeat timed out.\nStopped Sending Opus Packets and cleaned up.\n");
            fflush(stdout);
            recv_queue->queue_info->heartbeat_status = -1;
        }
    }
    return NULL;
}

_Noreturn void *handle_client(void *p_client_handler_args) {
    int **server_status = ((struct client_handler_info *) p_client_handler_args)->server_status;
    const int *current_clients_count = ((struct client_handler_info *) p_client_handler_args)->current_clients_count;

    pthread_mutex_t *complete_init_queue_mutex = ((struct client_handler_info *) p_client_handler_args)->complete_init_mutex[0];
    pthread_cond_t *complete_init_queue_cond = ((struct client_handler_info *) p_client_handler_args)->complete_init_cond[0];
    pthread_mutex_t *complete_init_client_mutex = ((struct client_handler_info *) p_client_handler_args)->complete_init_mutex[1];
    pthread_cond_t *complete_init_client_cond = ((struct client_handler_info *) p_client_handler_args)->complete_init_cond[1];

    while (true) {
        pthread_mutex_lock(complete_init_queue_mutex);
        pthread_cond_wait(complete_init_queue_cond, complete_init_queue_mutex);
        pthread_mutex_unlock(complete_init_queue_mutex);

        TaskQueue **recv_queues = *((struct client_handler_info *) p_client_handler_args)->recv_queues;
        if (ready_sock_server_seq1(recv_queues[(*current_clients_count) - 1])) {
            if (ready_sock_server_seq2(recv_queues[(*current_clients_count) - 1],
                                       ((struct client_handler_info *) p_client_handler_args)->data_len)) {
                if (ready_sock_server_seq3(recv_queues[(*current_clients_count) - 1]))
                    printf("Preparing socket sequence has been Successfully Completed.");
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

        if (((struct client_handler_info *) p_client_handler_args)->stream_consumer != NULL)
            *((struct client_handler_info *) p_client_handler_args)->stop_consumer = true;

        printf("\nStarted Sending Opus Packets...\n");
        fflush(stdout);

        pthread_mutex_lock(complete_init_client_mutex);
        pthread_cond_signal(complete_init_client_cond);
        pthread_mutex_unlock(complete_init_client_mutex);

        pthread_t opus_sender;
        pthread_t heartbeat_checker;

        struct opus_sender_args *p_opus_sender_args = malloc(sizeof(struct opus_sender_args));
        p_opus_sender_args->server_status = server_status;
        p_opus_sender_args->recv_queue = recv_queues[(*current_clients_count) - 1];
        p_opus_sender_args->opus_frame = ((struct client_handler_info *) p_client_handler_args)->opus_frame;
        p_opus_sender_args->opus_sender_mutex = ((struct client_handler_info *) p_client_handler_args)->opus_sender_mutex;
        p_opus_sender_args->opus_sender_cond = ((struct client_handler_info *) p_client_handler_args)->opus_sender_cond;

        // Activate opus sender per client.
        pthread_create(&opus_sender, NULL, provide_20ms_opus_sender, (void *) p_opus_sender_args);
        pthread_create(&heartbeat_checker, NULL, check_heartbeat, (void *) recv_queues[(*current_clients_count) - 1]);
    }
}

int ra_server(int port, int fd, uint32_t data_len, int **status) {
    bool stop_consumer = false;
    bool stream_mode = (fd == 0);
    FILE *fin = fdopen(fd, "rb");

    pthread_t opus_timer;
    pthread_t stream_consumer;
    pthread_t *p_stream_consumer = &stream_consumer;
    pthread_mutex_t stream_consumer_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t stream_consumer_cond = PTHREAD_COND_INITIALIZER;

    void **p_opus_timer_args = calloc(sizeof(void *), WORD);
    p_opus_timer_args[0] = status;
    if (stream_mode) {
        void **p_stream_consumer_args = calloc(sizeof(void *), DWORD);

        p_stream_consumer_args[0] = fin;
        p_stream_consumer_args[1] = &stop_consumer;
        p_stream_consumer_args[2] = &stream_consumer_mutex;
        p_stream_consumer_args[3] = &stream_consumer_cond;

        p_opus_timer_args[1] = &stream_consumer_cond;

        // activate opus timer for consuming stream early.
        pthread_create(&opus_timer, NULL, provide_20ms_opus_timer, (void *) p_opus_timer_args);
        pthread_create(p_stream_consumer, NULL, consume_until_connection, (void *) p_stream_consumer_args);
    } else
        p_stream_consumer = NULL;

    struct sockaddr_in server_addr;
    int sock_fd = server_init_socket(&server_addr, port);

    //Set fd to non-blocking mode.
    int flags = fcntl(fileno(fin), F_GETFL, 0);
    fcntl(fileno(fin), F_SETFL, flags | O_NONBLOCK);

    int current_clients_count = 0;
    pthread_mutex_t complete_init_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t complete_init_client_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t opus_builder_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t opus_sender_mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_cond_t complete_init_queue_cond = PTHREAD_COND_INITIALIZER;
    pthread_cond_t complete_init_client_cond = PTHREAD_COND_INITIALIZER;
    pthread_cond_t opus_builder_cond = PTHREAD_COND_INITIALIZER;
    pthread_cond_t opus_sender_cond = PTHREAD_COND_INITIALIZER;

    struct task_scheduler_info task_scheduler_args;
    struct client_handler_info client_handler_args;

    task_scheduler_args.sock_fd = sock_fd;
    task_scheduler_args.current_clients_count = &current_clients_count;
    task_scheduler_args.recv_queues = malloc(sizeof(TaskQueue *));

    task_scheduler_args.complete_init_queue_mutex = &complete_init_queue_mutex;
    task_scheduler_args.complete_init_queue_cond = &complete_init_queue_cond;

    client_handler_args.server_status = status;
    client_handler_args.current_clients_count = &current_clients_count;
    client_handler_args.recv_queues = &task_scheduler_args.recv_queues;
    client_handler_args.data_len = data_len;

    client_handler_args.stream_consumer = p_stream_consumer;
    client_handler_args.stop_consumer = &stop_consumer;

    client_handler_args.complete_init_mutex[0] = &complete_init_queue_mutex;
    client_handler_args.complete_init_cond[0] = &complete_init_queue_cond;
    client_handler_args.complete_init_mutex[1] = &complete_init_client_mutex;
    client_handler_args.complete_init_cond[1] = &complete_init_client_cond;

    client_handler_args.opus_frame = malloc(sizeof(Task));
    client_handler_args.opus_sender_mutex = &opus_sender_mutex;
    client_handler_args.opus_sender_cond = &opus_sender_cond;

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

    pthread_mutex_lock(&complete_init_client_mutex);
    pthread_cond_wait(&complete_init_client_cond, &complete_init_client_mutex);
    pthread_mutex_unlock(&complete_init_client_mutex);

    int err;

    /* Create a new encoder state */
    OpusEncoder *encoder;
    encoder = opus_encoder_create((opus_int32) OPUS_AUDIO_SR, OPUS_AUDIO_CH,
                                  OPUS_APPLICATION,
                                  &err);
    if (err < 0) {
        printf("Error: failed to create an encoder - %s\n", opus_strerror(err));
        return EXIT_FAILURE;
    }

    if (opus_encoder_ctl(encoder,
                         OPUS_SET_BITRATE(OPUS_AUDIO_SR * OPUS_AUDIO_CH)) <
        0) {
        printf("Error: failed to set bitrate - %s\n", opus_strerror(err));
        return EXIT_FAILURE;
    }

    pthread_t opus_builder;
    /* Create opus builder arguments struct. */
    struct opus_builder_args *p_opus_builder_args = calloc(sizeof(struct opus_builder_args), BYTE);
    p_opus_builder_args->server_status = status;
    p_opus_builder_args->fin = fin;
    p_opus_builder_args->encoder = encoder;
    p_opus_builder_args->opus_frame = client_handler_args.opus_frame;
    p_opus_builder_args->opus_builder_mutex = stream_mode ? &stream_consumer_mutex : &opus_builder_mutex;
    p_opus_builder_args->opus_sender_mutex = &opus_sender_mutex;
    p_opus_builder_args->opus_builder_cond = stream_mode ? &stream_consumer_cond : &opus_builder_cond;
    p_opus_builder_args->opus_sender_cond = &opus_sender_cond;

    // Activate opus builder.
    pthread_create(&opus_builder, NULL, provide_20ms_opus_builder, (void *) p_opus_builder_args);

    if (!stream_mode) {
        // Activate opus timer.
        p_opus_timer_args[1] = &opus_builder_cond;
        pthread_create(&opus_timer, NULL, provide_20ms_opus_timer, (void *) p_opus_timer_args);
    }

    /* Wait for joining threads. */
    pthread_join(opus_builder, NULL);
    pthread_join(opus_timer, NULL);

    /* Cancel unending threads. */
    pthread_cancel(task_scheduler);

    /* Send EOS Packet to clients && Clean up. */
    for (int i = 0; i < current_clients_count; i++) {
        sendto(task_scheduler_args.sock_fd, EOS, strlen(EOS), 0,
               (const struct sockaddr *) &task_scheduler_args.recv_queues[i]->queue_info->client->client_addr,
               task_scheduler_args.recv_queues[i]->queue_info->client->socket_len);
        free(task_scheduler_args.recv_queues[i]->queue_info);
        free(task_scheduler_args.recv_queues[i]);
    }

    /* Destroy the encoder state */
    opus_encoder_destroy(encoder);

    /* Close audio stream. */
    fclose(fin);

    return EXIT_SUCCESS;
}
