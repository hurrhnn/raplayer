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

int client_init_socket(ra_client_t *client, uint64_t idx) {
    /* Create socket file descriptor. */
    if ((client->list[idx].sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        return RAPLAYER_SOCKET_CREATION_FAILED;

    client->list[idx].server_addr.sin_family = AF_INET;
    client->list[idx].server_addr.sin_port = htons(client->list[idx].port);

    struct hostent *hostent;
    struct in_addr **addr_list;

    if ((hostent = gethostbyname(client->list[idx].address)) == NULL) {
        return RA_CLIENT_CONNECTION_RESOLVE_FAILED;
    } else {
        addr_list = (struct in_addr **) hostent->h_addr_list;
        char *resolved_address = malloc(0x100);
        strcpy(resolved_address, inet_ntoa(*addr_list[0]));

        if (!inet_pton(AF_INET, resolved_address, &client->list[idx].server_addr.sin_addr))
            return RA_CLIENT_ADDRESS_CONVERSION_FAILED;
        free(resolved_address);
    }
    return 0;
}

uint32_t ready_sock_client_seq1(ra_client_t *client, uint64_t idx) {
    const int buffer_size = 6;
    char *buffer = calloc(buffer_size, BYTE);

    int sock_fd = client->list[idx].sock_fd;
    int server_len = sizeof(struct sockaddr);
    struct sockaddr *server_addr = (struct sockaddr *) &client->list[idx].server_addr;
    sendto(sock_fd, HELLO, sizeof(HELLO), 0, server_addr, server_len);

    // Receive PCM info from server.
    recvfrom(sock_fd, buffer, buffer_size, 0, NULL, NULL);
    int info_len = (int) strtol(buffer, NULL, 10);
    buffer = realloc(buffer, info_len);

    sendto(sock_fd, OK, sizeof(OK), 0, server_addr, server_len);
    recvfrom(sock_fd, buffer, info_len, 0, NULL, NULL);

    memcpy(&client->list[idx].channels, buffer, WORD);
    memcpy(&client->list[idx].sample_rate, buffer + WORD, DWORD);
    memcpy(&client->list[idx].bit_per_sample, buffer + WORD + DWORD, WORD);

    buffer = realloc(buffer, DWORD);
    sprintf(buffer, "%d", info_len);

    if (sendto(sock_fd, buffer, strlen(buffer), 0, server_addr, server_len) > 0) {
        memset(buffer, 0, DWORD);
        recvfrom(sock_fd, buffer, DWORD, 0, NULL, NULL);

        uint32_t orig_pcm_size = 0;
        memcpy(&orig_pcm_size, buffer, DWORD);

        sendto(sock_fd, OK, sizeof(OK), 0, server_addr, server_len);
        free(buffer);
        return 0;
    }
    free(buffer);
    return RA_CLIENT_SOCKET_INIT_SEQ1_FAILED;
}

unsigned char *ready_sock_client_seq2(ra_client_t *client, uint64_t idx) {
    int sock_fd = client->list[idx].sock_fd;

    const int crypto_payload_size = CHACHA20_NONCEBYTES + CHACHA20_KEYBYTES;
    unsigned char *crypto_payload = malloc(crypto_payload_size);

    if (crypto_payload_size ==
        recvfrom(sock_fd, crypto_payload, crypto_payload_size, 0, NULL, NULL)) {
        sendto(sock_fd, OK, sizeof(OK), 0, (struct sockaddr *) &client->list[idx].server_addr, sizeof(struct sockaddr));
        return crypto_payload;
    }
    return (unsigned char *) RA_CLIENT_SOCKET_INIT_SEQ2_FAILED;
}

void *send_heartbeat(void *p_client) {
    ra_client_t *client = p_client;
    struct timespec timespec;
    timespec.tv_sec = 0;
    timespec.tv_nsec = 250000000;

    while (!(client->list[0].status & RA_NODE_CONNECTION_EXHAUSTED)) {
        sendto(client->list[0].sock_fd, HEARTBEAT, sizeof(HEARTBEAT), 0,
               (struct sockaddr *) &client->list[0].server_addr,
               sizeof(struct sockaddr));
        nanosleep(&timespec, NULL);
    }
    return EXIT_SUCCESS;
}

void* ra_client(void *p_client) {
    ra_client_t *client = p_client;
    uint64_t idx = client->idx;
    client->list[idx].status |= RA_NODE_INITIATED;

    int err;
    err = client_init_socket(client, idx);
    if(err != 0)
        return (void *) (uintptr_t) err;

    err = (int) ready_sock_client_seq1(client, idx);
    if(err == RA_CLIENT_SOCKET_INIT_SEQ1_FAILED)
        return (void *) (uintptr_t) err;

    struct chacha20_context ctx;
    unsigned char *crypto_payload = ready_sock_client_seq2(client, idx);
    if((int)(uintptr_t) crypto_payload == RA_CLIENT_SOCKET_INIT_SEQ2_FAILED)
        return (void *) (uintptr_t) crypto_payload;

    OpusDecoder *decoder; /* Create a new decoder state */
    decoder = opus_decoder_create(client->list[idx].sample_rate, client->list[idx].channels, &err);
    if (err < 0)
        return (void *) RA_CLIENT_CREATE_OPUS_DECODER_FAILED;

    client->list[idx].status |= RA_NODE_CONNECTED;
    pthread_t heartbeat_sender;
    pthread_create(&heartbeat_sender, NULL, send_heartbeat, (void *) client); // Activate heartbeat sender. //TODO: MEMFIX
    chacha20_init_context(&ctx, crypto_payload, crypto_payload + CHACHA20_NONCEBYTES, 0);

    unsigned char c_bits[MAX_DATA_SIZE];
    unsigned char **calculated_c_bits = malloc(sizeof(void *));
    opus_int16 *out = malloc(FRAME_SIZE * client->list[idx].channels);
    unsigned char *pcm_bytes = malloc(FRAME_SIZE * BYTE * client->list[idx].channels);
    while (true) {
        recvfrom(client->list[idx].sock_fd, c_bits, sizeof(c_bits), 0, NULL, NULL);
        if (c_bits[0] == 'E' && c_bits[1] == 'O' && c_bits[2] == 'S') { // Detect End of Stream.
            client->list[idx].status = RA_NODE_CONNECTION_EXHAUSTED;
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
//            printf("Error: Opus decoder failed - %s\n", opus_strerror(frame_size));
            client->list[idx].status = RA_NODE_CONNECTION_EXHAUSTED;
            return (void *) RA_CLIENT_OPUS_DECODE_FAILED;
        }

        /* Convert to little-endian ordering. */
        for (int i = 0; i < client->list[idx].channels * frame_size; i++) {
            pcm_bytes[2 * i] = out[i] & 0xFF;
            pcm_bytes[2 * i + 1] = (out[i] >> 8) & 0xFF;
        }
        // TODO: implement the client-side time synchronized callback
        client->list[idx].frame_callback(pcm_bytes, frame_size, client->list[idx].callback_user_data);
    }
    client->list[idx].status = RA_NODE_CONNECTION_EXHAUSTED;

    /* Cleaning up. */
    free(calculated_c_bits);

    /* Wait for a joining thread. */
    pthread_join(heartbeat_sender, NULL);

    /* Destroy the decoder state */
    opus_decoder_destroy(decoder);
    return 0;
}
