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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <opus/opus.h>
#include "chacha20/chacha20.h"

#define BYTE 1
#define WORD 2
#define DWORD 4

#define OK "OK"
#define OPUS_FLAG "OPUS"

#define FRAME_SIZE 960
#define MAX_PACKET_SIZE (3 * 1275)

// Max opus frame size if 1275 as from RFC6716.

// If sample <= 20ms opus_encode return always an one frame packet.
// If celt is used and sample is 40 or 60ms, two or three frames packet is generated as max celt frame size is 20ms.
// in this very specific case, the max packet size is multiplied by 2 or 3 respectively.

#define APPLICATION OPUS_APPLICATION_AUDIO

#define EOS "EOS" // End of Stream flag.

struct pcm_header {
    char chunk_id[4];
    uint32_t chunk_size;
    char format[4];
};

struct pcm_fmt_chunk {
    char chunk_id[4];
    uint32_t chunk_size;

    uint16_t audio_format;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byte_rate;

    uint16_t block_align;
    uint16_t bits_per_sample;
};

struct pcm_data_chunk {
    char chunk_id[4];
    uint32_t chunk_size;
    char *data;
};

struct pcm {
    struct pcm_header pcmHeader;
    struct pcm_fmt_chunk pcmFmtChunk;
    struct pcm_data_chunk pcmDataChunk;
};

struct client_socket_info {
    int sock_fd;
    struct sockaddr_in *client_addr;
    int *socket_len;
};

void init_pcm_structure(FILE *fin, struct pcm *pPcm, fpos_t *before_data_pos) {
    fread(pPcm->pcmHeader.chunk_id, DWORD, 1, fin);
    fread(&pPcm->pcmHeader.chunk_size, DWORD, 1, fin);
    fread(pPcm->pcmHeader.format, DWORD, 1, fin);

    fread(pPcm->pcmFmtChunk.chunk_id, DWORD, 1, fin);
    fread(&pPcm->pcmFmtChunk.chunk_size, DWORD, 1, fin);
    fread(&pPcm->pcmFmtChunk.audio_format, WORD, 1, fin);
    fread(&pPcm->pcmFmtChunk.channels, WORD, 1, fin);
    fread(&pPcm->pcmFmtChunk.sample_rate, DWORD, 1, fin);
    fread(&pPcm->pcmFmtChunk.byte_rate, DWORD, 1, fin);
    fread(&pPcm->pcmFmtChunk.block_align, WORD, 1, fin);
    fread(&pPcm->pcmFmtChunk.bits_per_sample, WORD, 1, fin);

    char tmpBytes[BYTE];
    while (1) {
        if (feof(fin)) { // End Of File.
            fprintf(stdout, "Error: The data chunk of the file not found.\n");
            exit(EXIT_FAILURE);
        }

        if (fread(tmpBytes, BYTE, 1, fin) && tmpBytes[0] == 'd')
            if (fread(tmpBytes, BYTE, 1, fin) && tmpBytes[0] == 'a')
                if (fread(tmpBytes, BYTE, 1, fin) && tmpBytes[0] == 't')
                    if (fread(tmpBytes, BYTE, 1, fin) && tmpBytes[0] == 'a')  // A PCM *d a t a* signature.
                        break;
    }

    strcpy(pPcm->pcmDataChunk.chunk_id, tmpBytes);
    fread(&pPcm->pcmDataChunk.chunk_size, DWORD, 1, fin);

    fgetpos(fin, before_data_pos); // Save begin of pcm data position.

    long pcm_data_size = pPcm->pcmDataChunk.chunk_size;
    pPcm->pcmDataChunk.data = malloc(BYTE * pcm_data_size);
    fread(pPcm->pcmDataChunk.data, pcm_data_size, 1, fin);
}

int server_init_socket(struct sockaddr_in *p_server_addr, int port) {

    struct sockaddr_in server_addr = *p_server_addr;
    int sock_fd;

    // Creating socket file descriptor.
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        fprintf(stdout, "Socket Creation Failed.\n");
        exit(EXIT_FAILURE);
    }

    memset((char *) &server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the socket with the server address.
    if (bind(sock_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        fprintf(stdout, "Socket Bind Failed.\n");
        exit(EXIT_FAILURE);
    }

    return sock_fd;
}

void server_init_sock_buffer(char *buffer, int size) {
    buffer = realloc(buffer, size);
    memset(buffer, 0, size);
}

int ready_sock_server_seq1(struct client_socket_info *p_client_socket_info) {

    struct sockaddr_in client_addr = *p_client_socket_info->client_addr;
    int buffer_size = 6;
    char *buffer = calloc(buffer_size, BYTE);

    recvfrom(p_client_socket_info->sock_fd, buffer, buffer_size, 0, (struct sockaddr *) &client_addr,
             (socklen_t *) p_client_socket_info->socket_len);
    buffer[strlen(buffer) - 1] = '\0';

    //print details of the client.
    printf("Connection from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    fflush(stdout);

    *p_client_socket_info->client_addr = client_addr;
    if (strncmp(buffer, "HELLO", strlen(buffer)) == 0)
        return sizeof(client_addr);
    else
        return 0;
}

bool ready_sock_server_seq2(struct pcm pcm_struct, struct client_socket_info *p_client_socket_info) {
    struct sockaddr_in client_addr = *p_client_socket_info->client_addr;

    int stream_info_size = DWORD + WORD * 2, buffer_size = 5;
    char *stream_info = malloc(stream_info_size);
    memcpy(stream_info, &pcm_struct.pcmFmtChunk.channels, WORD);
    memcpy((stream_info + WORD), &pcm_struct.pcmFmtChunk.sample_rate, DWORD);
    memcpy((stream_info + WORD + DWORD), &pcm_struct.pcmFmtChunk.bits_per_sample, WORD);

    char *buffer = calloc(buffer_size, BYTE);

    sprintf(buffer, "%d", stream_info_size);
    sendto(p_client_socket_info->sock_fd, buffer, buffer_size, 0, (struct sockaddr *) &client_addr,
           (socklen_t) *p_client_socket_info->socket_len);
    server_init_sock_buffer(buffer, buffer_size);

    recvfrom(p_client_socket_info->sock_fd, buffer, buffer_size, 0, NULL, NULL);
    if (strcmp(buffer, "OK") != 0) {
        fflush(stdout);
        return 0;
    }

    buffer_size += 5;
    server_init_sock_buffer(buffer, buffer_size);

    if (stream_info_size ==
        sendto(p_client_socket_info->sock_fd, stream_info, stream_info_size, 0,
               (struct sockaddr *) &client_addr, (socklen_t) *p_client_socket_info->socket_len)) {
        if (recvfrom(p_client_socket_info->sock_fd, buffer, buffer_size, 0, NULL, NULL) &&
            stream_info_size == strtol(buffer, NULL, 10)) {

            buffer_size = DWORD;
            server_init_sock_buffer(buffer, buffer_size);

            sendto(p_client_socket_info->sock_fd, &pcm_struct.pcmDataChunk.chunk_size, DWORD, 0,
                   (struct sockaddr *) &client_addr,
                   (socklen_t) *p_client_socket_info->socket_len);

            recvfrom(p_client_socket_info->sock_fd, buffer, DWORD, 0, NULL, NULL);
            if (strcmp(buffer, "OK") == 0)
                return true;
        }
    }
    return false;
}

bool ready_sock_server_seq3(unsigned char *crypto_payload, struct client_socket_info *p_client_socket_info) {
    struct sockaddr_in client_addr = *p_client_socket_info->client_addr;
    const int crypto_payload_size = crypto_stream_chacha20_NONCEBYTES + crypto_stream_chacha20_KEYBYTES;

    if (crypto_payload_size ==
        sendto(p_client_socket_info->sock_fd, crypto_payload, crypto_payload_size, 0, (struct sockaddr *) &client_addr,
               (socklen_t) *p_client_socket_info->socket_len)) {
        unsigned char *buffer = malloc(sizeof(OK));
        recvfrom(p_client_socket_info->sock_fd, buffer, sizeof(OK), 0, NULL, NULL);
        if (!strcmp((const char *) buffer, OK))
            return true;
    }
    return false;
}

int is_20ms = false;

void *provide_20ms_opus_timer() {
    while (1) {
        if (is_20ms == -1)
            break;

        if (!is_20ms) {
            usleep(19925);
            is_20ms = true;
        }
    }
}

void
provide_20ms_opus_stream(unsigned char *c_bits, int nbBytes, unsigned char *crypto_payload,
                         struct client_socket_info *p_client_socket_info) {
    struct sockaddr_in client_addr = *p_client_socket_info->client_addr;

    const int nbBytes_len = floor(log10(nbBytes) + 1);
    char *buffer = calloc(nbBytes_len + sizeof(OPUS_FLAG) + MAX_PACKET_SIZE, WORD);

    sprintf(buffer, "%d", nbBytes);
    strncat(buffer, OPUS_FLAG, sizeof(OPUS_FLAG));

    struct chacha20_context ctx;
    chacha20_init_context(&ctx, crypto_payload, crypto_payload + crypto_stream_chacha20_NONCEBYTES, 0);
    chacha20_xor(&ctx, c_bits, MAX_PACKET_SIZE);

    memcpy(buffer + nbBytes_len + sizeof(OPUS_FLAG) - 1, c_bits, MAX_PACKET_SIZE);

    while (true) {
        if (is_20ms) {
            sendto(p_client_socket_info->sock_fd, buffer, nbBytes_len + sizeof(OPUS_FLAG) + MAX_PACKET_SIZE, 0,
                   (const struct sockaddr *) &client_addr,
                   (socklen_t) *p_client_socket_info->socket_len); // Send opus stream.
            is_20ms = false;
            break;
        }
    }
    free(buffer);
}

void server_signal_timer(int signal) {
    puts("\nClient has been interrupted raplayer. Program now Exit.");
    exit(signal);
}

_Noreturn void *recv_heartbeat(void *p_sock_fd) {
    int sock_fd = *(int *) p_sock_fd;
    char buffer[2];

    while (true) {
        alarm(2);
        recvfrom(sock_fd, buffer, sizeof(buffer), 0, NULL, NULL);
    }
}

int ra_server(int argc, char **argv) {
    bool pipe_mode = false, stream_mode = false;
    signal(SIGALRM, server_signal_timer);

    if (argc < 3 || (strcmp(argv[2], "help") == 0)) {
        puts("");
        printf("Usage: %s --server [--stream] <FILE> [Port]\n\n", argv[0]);
        puts("<FILE>: The name of the wav file to play. (\"-\" to receive from STDIN)");

        puts("[--stream]: Allows flushing STDIN pipe when client connected. (prevent stacking buffer)");
        puts("[Port]: The port on the server to which you want to open.");
        puts("");
        return 0;
    }
    int port;
    if (argc < 5) {
        if (!strcmp(argv[2], "--stream"))
            stream_mode = true;
        port = 3845;
    } else if (strtol(argv[3], NULL, 10) != EINVAL && strtol(argv[3], NULL, 10)) {
        if (!strcmp(argv[2], "--stream"))
            stream_mode = true;
        port = (int) strtol(argv[3], NULL, 10);
    } else {
        if (!strcmp(argv[2], "--stream"))
            stream_mode = true;
        port = (int) strtol(argv[4], NULL, 10);
    }

    struct pcm *pcm_struct = malloc(sizeof(struct pcm));
    memset(pcm_struct, 0, sizeof(struct pcm));

    char *fin_name = stream_mode ? argv[3] : argv[2];
    if (fin_name[0] == '-' && fin_name[1] != '-') {
        pcm_struct->pcmFmtChunk.channels = 2;
        pcm_struct->pcmFmtChunk.sample_rate = 48000;
        pcm_struct->pcmFmtChunk.bits_per_sample = 16;
        pcm_struct->pcmDataChunk.chunk_size = 0;

        fin_name = "STDIN";
        pipe_mode = true;
    }

    if (!pipe_mode && stream_mode) {
        fprintf(stdout, "Invalid argument: --stream argument cannot run without <file> argument \"-\".\n");
        return EXIT_FAILURE;
    }

    FILE *fin;
    fin = (pipe_mode ? stdin : fopen(fin_name, "rb"));

    if (fin == NULL) {
        fprintf(stdout, "Error: Failed to open input file: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    fpos_t before_data_pos;
    if (!pipe_mode)
        init_pcm_structure(fin, pcm_struct, &before_data_pos);

    printf("\nFile %s info: \n", fin_name);
    printf("Channels: %hd\n", pcm_struct->pcmFmtChunk.channels);
    printf("Sample rate: %u\n", pcm_struct->pcmFmtChunk.sample_rate);
    printf("Bit per sample: %hd\n", pcm_struct->pcmFmtChunk.bits_per_sample);
    if (pipe_mode)
        printf("PCM data length: STDIN\n\n");
    else
        printf("PCM data length: %u\n\n", pcm_struct->pcmDataChunk.chunk_size);

    printf("Waiting for Client... ");
    fflush(stdout);

    struct sockaddr_in server_addr, client_addr;
    int sock_fd = server_init_socket(&server_addr, port), socket_len = sizeof(client_addr);

    struct client_socket_info *client_socket_info = malloc(sizeof(struct client_socket_info));
    client_socket_info->sock_fd = sock_fd, client_socket_info->client_addr = &server_addr, client_socket_info->socket_len = &socket_len;

    unsigned char *crypto_payload = generate_random_bytestream(
            crypto_stream_chacha20_NONCEBYTES + crypto_stream_chacha20_KEYBYTES);

    if ((socket_len = ready_sock_server_seq1(client_socket_info))) {
        alarm(5); // Start time-out timer.
        if (ready_sock_server_seq2(*pcm_struct, client_socket_info)) {
            if (ready_sock_server_seq3(crypto_payload, client_socket_info))
                printf("Preparing socket sequence has been Successfully Completed.");
            else {
                printf("Error: A crypto preparation sequence Failed.\n");
                return EXIT_FAILURE;
            }
        } else {
            printf("Error: A server socket preparation sequence Failed.\n");
            return EXIT_FAILURE;
        }
    } else {
        printf("Error: A client socket preparation sequence Failed.\n");
        return EXIT_FAILURE;
    }
    printf("%s", pipe_mode ? "\n\nStarted Sending Opus Packets... Press any key to stop audio streaming.\n"
                           : "\nStarted Sending Opus Packets...\n");
    fflush(stdout);

    opus_int16 in[FRAME_SIZE * pcm_struct->pcmFmtChunk.channels];
    unsigned char c_bits[MAX_PACKET_SIZE];

    OpusEncoder *encoder;
    int err;

    /*Create a new encoder state */
    encoder = opus_encoder_create((opus_int32) pcm_struct->pcmFmtChunk.sample_rate, pcm_struct->pcmFmtChunk.channels,
                                  APPLICATION,
                                  &err);
    if (err < 0) {
        fprintf(stdout, "failed to create an encoder: %s\n", opus_strerror(err));
        return EXIT_FAILURE;
    }

    if (opus_encoder_ctl(encoder,
                         OPUS_SET_BITRATE(pcm_struct->pcmFmtChunk.sample_rate * pcm_struct->pcmFmtChunk.channels)) <
        0) {
        fprintf(stdout, "failed to set bitrate: %s\n", opus_strerror(err));
        return EXIT_FAILURE;
    }

    if (!pipe_mode)
        fsetpos(fin, &before_data_pos); // Re-read pcm data bytes from stream.

    if (stream_mode) {
        fseek(stdin, 0, SEEK_END);
        fcntl(fileno(fin), F_SETFL, fcntl(fileno(fin), F_GETFL) | O_NONBLOCK);
    }

    pthread_t opus_timer, heartbeat_receiver;
    pthread_create(&opus_timer, NULL, provide_20ms_opus_timer, NULL); // Activate opus timer.
    pthread_create(&heartbeat_receiver, NULL, recv_heartbeat, &sock_fd);

    int nbBytes;
    while (1) {
        unsigned char pcm_bytes[FRAME_SIZE * pcm_struct->pcmFmtChunk.channels * WORD];

        /* Read a 16 bits/sample audio frame. */
        fread(pcm_bytes, WORD * pcm_struct->pcmFmtChunk.channels, FRAME_SIZE, fin);
        if (feof(fin)) // End Of Stream.
            break;

        /* Convert from little-endian ordering. */
        for (int i = 0; i < WORD * pcm_struct->pcmFmtChunk.channels * FRAME_SIZE; i++)
            in[i] = (opus_int16) (pcm_bytes[2 * i + 1] << 8 | pcm_bytes[2 * i]);

        /* Encode the frame. */
        nbBytes = opus_encode(encoder, in, FRAME_SIZE, c_bits,
                              MAX_PACKET_SIZE);
        if (nbBytes < 0) {
            fprintf(stdout, "encode failed: %s\n", opus_strerror(nbBytes));
            return EXIT_FAILURE;
        }

        // Send encoded PCM data.
        provide_20ms_opus_stream(c_bits, nbBytes, crypto_payload, client_socket_info);
    }
    // Send EOS Packet.
    sendto(sock_fd, EOS, strlen(EOS), 0, (struct sockaddr *) &client_addr, socket_len);
    is_20ms = -1;

    /* Destroy the encoder state */
    opus_encoder_destroy(encoder);

    /* Close file stream. */
    fclose(fin);
    return EXIT_SUCCESS;
}
