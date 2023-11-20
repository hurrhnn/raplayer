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
#include <raplayer/chacha20.h>
#include <raplayer/task_queue.h>
#include <raplayer/utils.h>

struct stream_info {
    int16_t channels;
    int32_t sample_rate;
    int16_t bits_per_sample;
};

struct server_socket_info {
    int **client_status;
    int sock_fd;
    struct sockaddr_in *server_addr;
    int *socket_len;
};

// TODO: make `-1` to error code
int client_init_socket(char *server_addr, int server_port, struct sockaddr_in *p_ctx_server_addr) {
    struct sockaddr_in ctx_server_addr = *p_ctx_server_addr;
    int sock_fd;

    // Creating socket file descriptor.
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        fprintf(stdout, "Error: Socket Creation Failed.\n");
        return -1;
    }

    memset((char *) &ctx_server_addr, 0, sizeof(ctx_server_addr));
    ctx_server_addr.sin_family = AF_INET; // IPv4
    ctx_server_addr.sin_port = htons((uint16_t) server_port);

    struct hostent *hostent;
    struct in_addr **addr_list;

    if ((hostent = gethostbyname(server_addr)) == NULL) {
        printf("Error: Connection Cannot resolved to %s.\n", server_addr);
        return -1;
    } else {
        addr_list = (struct in_addr **) hostent->h_addr_list;
        strcpy(server_addr, inet_ntoa(*addr_list[0]));
    }

    if (!inet_pton(AF_INET, server_addr, &ctx_server_addr.sin_addr)) {
        puts("Error: Convert Internet host address Failed.");
        return -1;
    }

    *p_ctx_server_addr = ctx_server_addr;
    return sock_fd;
}

uint32_t ready_sock_client_seq1(struct stream_info *streamInfo, const struct server_socket_info *p_server_socket_info) {
    struct sockaddr_in server_addr = *p_server_socket_info->server_addr;
    const int buffer_size = 6;
    char *buffer = calloc(buffer_size, BYTE);

    sendto(p_server_socket_info->sock_fd, HELLO, sizeof(HELLO), 0, (struct sockaddr *) &server_addr,
           *p_server_socket_info->socket_len);

    // Receive PCM info from server.
    recvfrom(p_server_socket_info->sock_fd, buffer, buffer_size, 0, NULL, NULL);
    int info_len = (int) strtol(buffer, NULL, 10);
    buffer = realloc(buffer, info_len);

    sendto(p_server_socket_info->sock_fd, OK, sizeof(OK), 0, (struct sockaddr *) &server_addr,
           *p_server_socket_info->socket_len);
    recvfrom(p_server_socket_info->sock_fd, buffer, info_len, 0, NULL, NULL);

    memcpy(&streamInfo->channels, buffer, WORD);
    memcpy(&streamInfo->sample_rate, buffer + WORD, DWORD);
    memcpy(&streamInfo->bits_per_sample, buffer + WORD + DWORD, WORD);

    buffer = realloc(buffer, DWORD);
    sprintf(buffer, "%d", info_len);

    if (sendto(p_server_socket_info->sock_fd, buffer, strlen(buffer), 0, (struct sockaddr *) &server_addr,
               *p_server_socket_info->socket_len) > 0) {
        memset(buffer, 0, DWORD);
        recvfrom(p_server_socket_info->sock_fd, buffer, DWORD, 0, NULL, NULL);

        uint32_t orig_pcm_size = 0;
        memcpy(&orig_pcm_size, buffer, DWORD);

        sendto(p_server_socket_info->sock_fd, OK, sizeof(OK), 0, (struct sockaddr *) &server_addr,
               *p_server_socket_info->socket_len);
        return orig_pcm_size;
    }
    free(buffer);
    return 0;
}

unsigned char *ready_sock_client_seq2(const struct server_socket_info *p_server_socket_info) {
    struct sockaddr_in server_addr = *p_server_socket_info->server_addr;
    const int crypto_payload_size = CHACHA20_NONCEBYTES + CHACHA20_KEYBYTES;
    unsigned char *crypto_payload = malloc(crypto_payload_size);

    if (crypto_payload_size ==
        recvfrom(p_server_socket_info->sock_fd, crypto_payload, crypto_payload_size, 0, NULL, NULL)) {
        sendto(p_server_socket_info->sock_fd, OK, sizeof(OK), 0, (struct sockaddr *) &server_addr,
               (socklen_t) *p_server_socket_info->socket_len);
        return crypto_payload;
    }
    return NULL;
}

void *send_heartbeat(void *p_server_socket_info) {
    while (!**((struct server_socket_info *) p_server_socket_info)->client_status) {
        sendto(((struct server_socket_info *) p_server_socket_info)->sock_fd, HEARTBEAT, sizeof(HEARTBEAT), 0,
               (struct sockaddr *) ((struct server_socket_info *) p_server_socket_info)->server_addr,
               *((struct server_socket_info *) p_server_socket_info)->socket_len);

        struct timespec timespec;
        timespec.tv_sec = 0;
        timespec.tv_nsec = 250000000;
        nanosleep(&timespec, NULL);
    }
    return EXIT_SUCCESS;
}

int ra_client(char *address, int port, void (*frame_callback)(void *frame, int frame_size, void *user_data),
              void *callback_user_data, int **client_status) {
    int err;
    struct stream_info pStreamInfo;
    struct sockaddr_in server_addr;

    int sock_fd = client_init_socket(address, port, &server_addr);
    int socket_len = sizeof(server_addr);

    struct server_socket_info server_socket_info;
    server_socket_info.client_status = client_status;
    server_socket_info.sock_fd = sock_fd;
    server_socket_info.server_addr = &server_addr;
    server_socket_info.socket_len = &socket_len;

    ready_sock_client_seq1(&pStreamInfo, &server_socket_info);

    struct chacha20_context ctx;
    unsigned char *crypto_payload = ready_sock_client_seq2(&server_socket_info);

    OpusDecoder *decoder; /* Create a new decoder state */
    decoder = opus_decoder_create(pStreamInfo.sample_rate, pStreamInfo.channels, &err);
    if (err < 0) {
        printf("Error: failed to create an decoder - %s\n", opus_strerror(err));
        return EXIT_FAILURE;
    }

    **client_status = 0;
    pthread_t heartbeat_sender;
    pthread_create(&heartbeat_sender, NULL, send_heartbeat, (void *) &server_socket_info); // Activate heartbeat sender.
    chacha20_init_context(&ctx, crypto_payload, crypto_payload + CHACHA20_NONCEBYTES, 0);
    unsigned char **calculated_c_bits = malloc(sizeof(void *));

    while (1) {
        unsigned char c_bits[MAX_DATA_SIZE];

        opus_int16 out[FRAME_SIZE * pStreamInfo.channels];
        unsigned char pcm_bytes[FRAME_SIZE * pStreamInfo.channels * WORD];

        recvfrom(sock_fd, c_bits, sizeof(c_bits), 0, NULL, NULL);
        if (**client_status || (c_bits[0] == 'E' && c_bits[1] == 'O' && c_bits[2] == 'S')) { // Detect End of Stream.
            **client_status = 1;
            break;
        }

        // Calculate opus frame offset.
        uint64_t nbBytes = provide_20ms_opus_offset_calculator(c_bits, calculated_c_bits);

        /* Decrypt the frame. */
        chacha20_xor(&ctx, *calculated_c_bits, nbBytes);

        /* Decode the frame. */
        int frame_size = opus_decode(decoder, (unsigned char *) *calculated_c_bits, (opus_int32) nbBytes, out,
                                     FRAME_SIZE, 0);
        if (frame_size < 0) {
            printf("Error: Opus decoder failed - %s\n", opus_strerror(frame_size));
            return EXIT_FAILURE;
        }

        /* Convert to little-endian ordering. */
        for (int i = 0; i < pStreamInfo.channels * frame_size; i++) {
            pcm_bytes[2 * i] = out[i] & 0xFF;
            pcm_bytes[2 * i + 1] = (out[i] >> 8) & 0xFF;
        }
        // TODO: implement the client-side time synchronized callback
        frame_callback(pcm_bytes, frame_size, callback_user_data);
    }
    /* Cleaning up */
    free(calculated_c_bits);

    /* Wait for a joining thread. */
    pthread_join(heartbeat_sender, NULL);

    /* Destroy the decoder state */
    opus_decoder_destroy(decoder);
    return 0;
}
