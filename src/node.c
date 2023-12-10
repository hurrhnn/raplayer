#include <raplayer/node.h>

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
#include <raplayer/chacha20.h>
#include <raplayer/queue.h>
#include <raplayer/scheduler.h>
#include <raplayer/utils.h>
#include <raplayer/rtp.h>

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
                RA_WARN("%02llu: Connection closed from %s:%d\n",
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

    void* heartbeat_msg = malloc(DWORD + BYTE);
    memcpy(heartbeat_msg, RA_CTL_HEADER, DWORD);
    memcpy(heartbeat_msg + DWORD, "\x00", BYTE);

    while (!(node->status & RA_NODE_CONNECTION_EXHAUSTED)) {
        sendto((int) node->local_sock->fd, heartbeat_msg, DWORD + BYTE, 0,
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

    /* Create a new encoder state. */
    int err;
    OpusEncoder *encoder;
    encoder = opus_encoder_create((opus_int32) OPUS_AUDIO_SR, OPUS_AUDIO_CH,
                                  OPUS_APPLICATION,
                                  &err);
    if (err < 0) {
        RA_ERROR("Error: failed to create an encoder - %s\n", opus_strerror(err));
        node->status = RA_NODE_CONNECTION_EXHAUSTED;
        return (void *) RA_CREATE_OPUS_ENCODER_FAILED;
    }

    if (opus_encoder_ctl(encoder,
                         OPUS_SET_BITRATE(OPUS_AUDIO_SR * OPUS_AUDIO_CH)) <
        0) {
        RA_ERROR("Error: failed to set bitrate - %s\n", opus_strerror(err));
        node->status = RA_NODE_CONNECTION_EXHAUSTED;
        return (void *) RA_OPUS_ENCODER_CTL_FAILED;
    }

    opus_int16 in[FRAME_SIZE * OPUS_AUDIO_CH];
    unsigned char c_bits[FRAME_SIZE]; // TODO: need to change for correspond adaptive latency

    uint32_t sequence = 0;
    uint32_t *ssrc = (uint32_t *) generate_random_bytestream(sizeof(uint32_t));
    uint32_t timestamp = 0;

    ra_spawn_t *node_spawn = NULL, **spawn = *opus_builder_args->spawn;
    for(int i = 0; i < *opus_builder_args->cnt_spawn; i++) {
        if(spawn[i]->type == RA_SPAWN_TYPE_SEND &&
           ra_compare_sockaddr(&spawn[i]->local_sock->addr, &node->local_sock->addr))
        {
            node_spawn = spawn[i];
            break;
        }
    }

    if(node_spawn == NULL) {
        RA_DEBUG_MORE(YEL, "send not registered! sender will not active.\n");
        return NULL;
    }

    while (true) {
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

        /* Read 16bit/sample audio frame. */
        unsigned char *pcm_bytes = node_spawn->callback.send(node_spawn->cb_user_data);

        /* Convert from little-endian ordering. */
        for (int i = 0; i < OPUS_AUDIO_CH * FRAME_SIZE; i++)
            in[i] = (opus_int16) (pcm_bytes[2 * i + 1] << 8 | pcm_bytes[2 * i]);

        /* Encode the frame. */
        int nbBytes = opus_encode(encoder, in, FRAME_SIZE, c_bits, FRAME_SIZE);
        if (nbBytes < 0) {
            RA_ERROR("Error: opus encode failed - %s\n", opus_strerror(nbBytes));
            node->status = RA_NODE_CONNECTION_EXHAUSTED;
            return (void *) RA_OPUS_ENCODE_FAILED;
        }

        /* Create payload. */
        ra_rtp_t rtp_header;
        rtp_header.version = 2;
        rtp_header.padding = 0;
        rtp_header.extension = 0;
        rtp_header.csrc_count = 0;
        rtp_header.marker = 0;
        rtp_header.payload_type = 120;
        rtp_header.sequence = ra_swap_endian_uint16((++sequence) % 0x10000);
        rtp_header.timestamp = ra_swap_endian_uint32((timestamp = timestamp + 960) % 0x100000000);
        rtp_header.ssrc = ra_swap_endian_uint32(*ssrc);
        rtp_header.csrc = NULL;

        uint16_t rtp_header_len = sizeof(rtp_header.without_csrc_data) + (sizeof(rtp_header.csrc) * rtp_header.csrc_count);
        for(int i = 0; i < rtp_header.csrc_count; i++)
            rtp_header_len += (sizeof (*rtp_header.csrc));

        /* Encrypt the frame. */
        // chacha20_xor(&crypto_context, c_bits, nbBytes); /* use client's crypto context instead of shared context */
        // chacha20_xor(&client->crypto_context, opus_frame->data + nbBytes_len + sizeof(OPUS_FLAG) - 1,nbBytes);

        ra_task_t *opus_frame = create_task(MAX_DATA_SIZE);
        memset(opus_frame->data, 0x0, opus_frame->data_len);

        opus_frame->data_len = (ssize_t) (rtp_header_len + nbBytes);
        memcpy(opus_frame->data, rtp_header.without_csrc_data, rtp_header_len);
        memcpy(opus_frame->data + rtp_header_len, c_bits, opus_frame->data_len);

        /* Enqueue the payload. */
        append_task(node->send_queue, opus_frame);

        *opus_builder_args->turn = 1;
        pthread_cond_signal(opus_builder_args->opus_builder_cond);
        pthread_mutex_unlock(opus_builder_args->opus_builder_mutex);
    }

    /* Destroy the encoder state. */
    opus_encoder_destroy(encoder);
    return NULL;
}

void *ra_20ms_opus_sender(void *p_opus_sender_args) {
    ra_opus_sender_args_t *opus_sender_args = (ra_opus_sender_args_t *) p_opus_sender_args;
    while (!(opus_sender_args->node->status & RA_NODE_CONNECTION_EXHAUSTED)) {
        ra_task_t *opus_frame = retrieve_task(opus_sender_args->node->send_queue);
        pthread_mutex_lock(opus_sender_args->opus_sender_mutex);
        pthread_cond_wait(opus_sender_args->opus_sender_cond, opus_sender_args->opus_sender_mutex);
        sendto((int) opus_sender_args->node->local_sock->fd, opus_frame->data,
               opus_frame->data_len, 0, (struct sockaddr *) &opus_sender_args->node->remote_sock->addr, sizeof(struct sockaddr_in));
        pthread_mutex_unlock(opus_sender_args->opus_sender_mutex);
        remove_task(opus_frame);
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
    ra_node_t *node = ((ra_node_frame_args_t *) p_node_frame_args)->node;
    ra_spawn_t **spawn = *((ra_node_frame_args_t *) p_node_frame_args)->spawn;
    uint64_t *cnt_spawn = ((ra_node_frame_args_t *) p_node_frame_args)->cnt_spawn;

    int err;
    OpusDecoder *decoder; /* Create a new decoder state */
    decoder = opus_decoder_create(node->sample_rate, node->channels, &err);
    if (err < 0) {
        node->status = RA_NODE_CONNECTION_EXHAUSTED;
        return (void *) RA_CREATE_OPUS_DECODER_FAILED;
    }

//    chacha20_init_context(&ctx, crypto_payload, crypto_payload + CHACHA20_NONCEBYTES, 0);

    unsigned char *c_bits;
    opus_int16 *out = malloc(FRAME_SIZE * WORD * node->channels);
    unsigned char *pcm_bytes = malloc(FRAME_SIZE * WORD * node->channels);

    ra_spawn_t *node_spawn = NULL;
    for(int i = 0; i < *cnt_spawn; i++) {
        if(spawn[i]->type == RA_SPAWN_TYPE_RECV &&
        ra_compare_sockaddr(&spawn[i]->local_sock->addr, &node->local_sock->addr))
        {
            node_spawn = spawn[i];
            break;
        }
    }

    if(node_spawn == NULL) {
        RA_DEBUG_MORE(YEL, "recv not registered! receiver will not active.\n");
        return NULL;
    }

    while (true) {
        ra_task_t *task = retrieve_task(node->frame_queue);
        ra_rtp_t rtp_header = ra_get_rtp_context(task->data);
        uint64_t rtp_header_len = ra_get_rtp_length(rtp_header);

        c_bits = (task->data + rtp_header_len);
        if (c_bits[0] == 'E' && c_bits[1] == 'O' && c_bits[2] == 'S') // Detect End of Stream.
            break;

        /* Decrypt the frame. */
//        chacha20_xor(&ctx, *calculated_c_bits, nbBytes);

        /* Decode the frame. */
        int frame_size = opus_decode(decoder, (unsigned char *) c_bits,
                                     (opus_int32) (task->data_len - rtp_header_len), out,FRAME_SIZE, 0);
        if (frame_size < 0) {
            printf("Error: Opus decoder failed - %s\n", opus_strerror(frame_size));
            node->status = RA_NODE_CONNECTION_EXHAUSTED;
            return (void *) RA_OPUS_DECODE_FAILED;
        }

        /* Convert to little-endian ordering. */
        for (int i = 0; i < node->channels * frame_size; i++) {
            pcm_bytes[2 * i] = out[i] & 0xFF;
            pcm_bytes[2 * i + 1] = (out[i] >> 8) & 0xFF;
        }

        // TODO: implement the client-side time synchronized callback
        node_spawn->callback.recv(pcm_bytes, frame_size, node_spawn->cb_user_data);
        remove_task(task);
    }

    /* Wait for a joining thread. */
//    pthread_join(heartbeat_sender, NULL);

    /* Destroy the decoder state */
    opus_decoder_destroy(decoder);
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
    p_opus_builder_args->spawn = ((ra_node_frame_args_t *) p_node_frame_args)->spawn;
    p_opus_builder_args->cnt_spawn = ((ra_node_frame_args_t *) p_node_frame_args)->cnt_spawn;
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