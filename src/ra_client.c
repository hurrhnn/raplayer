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
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <opus/opus.h>
#include <portaudio.h>
#include <pthread.h>

#define BYTE 1
#define WORD 2
#define DWORD 4

#define HELLO "HELLO"
#define OK "OK"

#define OPUS_FLAG "OPUS"

#define MAX_FRAME_SIZE (6 * 960)
#define MAX_PACKET_SIZE (3 * 1275)

// Max opus frame size if 1275 as from RFC6716.

// If sample <= 20ms opus_encode return always an one frame packet.
// If celt is used and sample is 40 or 60ms, two or three frames packet is generated as max celt frame size is 20ms
// in this very specific case, the max packet size is multiplied by 2 or 3 respectively

struct stream_info {
    short channels;
    long sample_rate;
    short bits_per_sample;
};

struct server_socket_info {
    int sock_fd;
    struct sockaddr_in server_addr;
    int socket_len;
};

int client_init_socket(char *str_server_addr, int port, struct sockaddr_in *p_server_addr) {
    struct sockaddr_in server_addr = *p_server_addr;
    int sock_fd;

    // Creating socket file descriptor.
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        fprintf(stdout, "Error: Socket Creation Failed.\n");
        exit(EXIT_FAILURE);
    }

    memset((char *) &server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_port = htons(port);

    if (!inet_aton(str_server_addr, &server_addr.sin_addr)) {
        fprintf(stdout, "Error: Convert Internet host address Failed.\n");
        exit(EXIT_FAILURE);
    }

    *p_server_addr = server_addr;
    return sock_fd;
}

void client_init_sock_buffer(char *buffer, int size) {
    buffer = realloc(buffer, size);
    memset(buffer, 0, sizeof(&buffer));
}

long ready_sock_client_seq1(struct stream_info *streamInfo, int sock_fd, char *buffer, struct sockaddr_in *p_server_addr,
                            int socket_len) {
    struct sockaddr_in server_addr = *p_server_addr;
    sendto(sock_fd, HELLO, sizeof(HELLO), 0, (struct sockaddr *) &server_addr, socket_len);

    // Receive PCM info from server.
    recvfrom(sock_fd, buffer, sizeof(buffer), 0, NULL, NULL);
    int info_len = (int) strtol(buffer, NULL, 10);
    client_init_sock_buffer(buffer, info_len);

    sendto(sock_fd, OK, sizeof(OK), 0, (struct sockaddr *) &server_addr, socket_len);
    recvfrom(sock_fd, buffer, info_len, 0, NULL, NULL);

    memcpy(&streamInfo->channels, buffer, WORD);
    memcpy(&streamInfo->sample_rate, buffer + WORD, DWORD);
    memcpy(&streamInfo->bits_per_sample, buffer + WORD + DWORD, WORD);

    client_init_sock_buffer(buffer, 5);
    sprintf(buffer, "%d", info_len);

    if (sendto(sock_fd, buffer, strlen(buffer), 0, (struct sockaddr *) &server_addr, socket_len) > 0) {
        memset(buffer, 0, DWORD);
        recvfrom(sock_fd, buffer, DWORD, 0, NULL, NULL);

        long *orig_pcm_size = malloc(DWORD);
        memcpy(orig_pcm_size, buffer, DWORD);

        sendto(sock_fd, OK, sizeof(OK), 0, (struct sockaddr *) &server_addr, socket_len);
        return *orig_pcm_size;
    }
    return 0;
}

void client_signal_timer(int signal)
{
    exit(signal);
}

long EOS = 0, sum_frame_cnt = 0, sum_frame_size = 0;
void *print_info()
{
    int print_cnt = 0;
    char symbols[] = {'-', '\\', '|', '/'};
    puts("");

    while (!EOS) {
        printf("[%c] Elapsed time: %.2lfs, Received frame size: %.2lfKB\r", symbols[++print_cnt % DWORD], (double)(sum_frame_cnt * 20) / 1000, (double)(sum_frame_size) / 1000);
        fflush(stdout);
        usleep(250000);
    }
    return EXIT_SUCCESS;
}

void *send_heartbeat(void *p_server_socket_info)
{
    while (!EOS) {
        sendto(((struct server_socket_info *) p_server_socket_info)->sock_fd, OK, sizeof(OK), 0,
                (struct sockaddr *) &((struct server_socket_info *) p_server_socket_info)->server_addr,
                        ((struct server_socket_info *) p_server_socket_info)->socket_len);
        usleep(250000);
    }
    return EXIT_SUCCESS;
}

int ra_client(int argc, char **argv) {
    signal(SIGALRM, client_signal_timer);
    struct stream_info *pStreamInfo = calloc(sizeof(struct stream_info), BYTE);

    struct sockaddr_in server_addr;
    int port;

    if (argc < 3 || (strcmp(argv[2], "help") == 0)) {
        puts("");
        printf("Usage: %s --client <Server Address> [Port]\n\n", argv[0]);
        puts("<Server Address>: The IP or address of the server to which you want to connect.");
        puts("[Port]: The port on the server to which you want to connect.");
        puts("");
        return 0;
    }

    if (argc < 4)
        port = 3845;
    else
        port = (int) strtol(argv[3], NULL, 10);

    alarm(2); // Start time-out timer.
    char *buffer = (char *) calloc(6, BYTE);
    int sock_fd = client_init_socket(argv[2], port, (struct sockaddr_in *) &server_addr), socket_len = sizeof(server_addr);

    long orig_pcm_size = ready_sock_client_seq1((struct stream_info *) pStreamInfo, sock_fd, buffer, &server_addr, socket_len);

    printf("Received audio info: \n");
    printf("Channels: %hd\n", pStreamInfo->channels);
    printf("Sample rate: %ld\n", pStreamInfo->sample_rate);
    printf("Bit per sample: %hd\n", pStreamInfo->bits_per_sample);
    if(orig_pcm_size == 0)
        printf("PCM data length: STDIN\n\n");
    else
        printf("PCM data length: %ld\n\n", orig_pcm_size);
    fflush(stdout);

    PaStreamParameters outputParameters;
    PaStream *stream;
    int err;

    outputParameters.device = Pa_GetDefaultOutputDevice(); /* Get default output device */
    if (outputParameters.device == paNoDevice) {
        fprintf(stdout,"Error: No default output device.\n");
        exit(-1);
    }
    outputParameters.channelCount = pStreamInfo->channels;
    outputParameters.sampleFormat = paInt16; /* 16 bit integer output */
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device) -> defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    Pa_OpenStream(
            &stream,
            NULL, /* no input */
            &outputParameters,
            (double) pStreamInfo->sample_rate,
            paFramesPerBufferUnspecified,
            paClipOff, /* we won't output out of range samples so don't bother clipping them */
            NULL, /* no callback, use blocking I/O */
            NULL);

    OpusDecoder *decoder; /* Create a new decoder state */
    decoder = opus_decoder_create((opus_int32) pStreamInfo->sample_rate, pStreamInfo->channels, &err);
    if (err < 0) {
        fprintf(stdout, "failed to create an decoder: %s\n", opus_strerror(err));
        return EXIT_FAILURE;
    }

    printf("Preparing socket sequence has been Successfully Completed.");
    printf("\nStarted Playing Opus Packets...\n");
    fflush(stdout);

    pthread_t info_printer, heartbeat_sender;
    pthread_create(&info_printer, NULL, print_info, NULL); // Activate opus timer.

    struct server_socket_info *server_socket_info = (struct server_socket_info *)malloc(sizeof(struct server_socket_info));
    server_socket_info->sock_fd = sock_fd, server_socket_info->server_addr = server_addr, server_socket_info->socket_len = socket_len;
    pthread_create(&heartbeat_sender, NULL, send_heartbeat, (void *)server_socket_info);

    Pa_StartStream(stream);
    while (1) {
        alarm(2);
        unsigned char c_bits[DWORD + sizeof(OPUS_FLAG) + MAX_PACKET_SIZE];

        opus_int16 out[MAX_FRAME_SIZE * pStreamInfo->channels];
        unsigned char pcm_bytes[MAX_FRAME_SIZE * pStreamInfo->channels * WORD];

        recvfrom(sock_fd, c_bits, sizeof(c_bits), 0, NULL, NULL);
        if (c_bits[0] == 'E' && c_bits[1] == 'O' && c_bits[2] == 'S') { // Detect End of Stream.
            EOS = 1;
            break;
        }

        int idx = 0;
        for(int i = 0; i < sizeof(c_bits); i++) { // 'OPUS' indicates for Start of opus stream.
            if(c_bits[i] == 'O' && c_bits[i + 1] == 'P' && c_bits[i + 2] == 'U' && c_bits[i + 3] == 'S')
            {
                idx = i - 1;
                break;
            }
        }

        char str_nbBytes[idx + 1];
        for(int i = 0; i <= idx + 1; i++) {
            if(i == (idx + 1))
                str_nbBytes[i] = '\0';
            else
                str_nbBytes[i] = (char) c_bits[i];
        }
        long nbBytes = strtol(str_nbBytes, NULL, 10);

        /* Decode the frame. */
        int frame_size = opus_decode(decoder, (unsigned char *) c_bits + idx + 5, (opus_int32) nbBytes, out,
                                     MAX_FRAME_SIZE, 0);
        if (frame_size < 0) {
            fprintf(stdout, "decoder failed: %s\n", opus_strerror(frame_size));
            return EXIT_FAILURE;
        }

        /* Convert to little-endian ordering. */
        for (int i = 0; i < pStreamInfo->channels * frame_size; i++) {
            pcm_bytes[2 * i] = out[i] & 0xFF;
            pcm_bytes[2 * i + 1] = (out[i] >> 8) & 0xFF;
        }
        Pa_WriteStream(stream, pcm_bytes, frame_size);
        sum_frame_cnt++, sum_frame_size += nbBytes;
    }
    Pa_StopStream(stream);

    /* Destroy the decoder state */
    opus_decoder_destroy(decoder);

    /* Don't forget to clean up! */
    Pa_CloseStream(stream);
    Pa_Terminate();
    return 0;
}
