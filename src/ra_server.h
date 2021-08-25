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
#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <opus/opus.h>

#ifndef RAPLAYER_RA_SERVER_H
#define RAPLAYER_RA_SERVER_H

#define BYTE 1
#define WORD 2
#define DWORD 4

#define OK "OK"
#define OPUS_FLAG "OPUS"

#define FRAME_SIZE 960
#define MAX_DATA_SIZE 4096
#define APPLICATION OPUS_APPLICATION_AUDIO

#define EOS "EOS" // End of Stream FLAG.

#include "task_scheduler/task_scheduler.h"
#include "task_scheduler/task_queue/task_queue.h"

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

struct opus_builder_args {
    struct pcm *pcm_struct;
    FILE *fin;
    OpusEncoder *encoder;
    unsigned char *crypto_payload;

    pthread_mutex_t *opus_builder_mutex;
    pthread_mutex_t *opus_sender_mutex;

    pthread_cond_t *opus_builder_cond;
    pthread_cond_t *opus_sender_cond;
    Task *opus_frame;
};

struct opus_sender_args {
    TaskQueue *recv_queue;
    Task *opus_frame;

    pthread_mutex_t *opus_sender_mutex;
    pthread_cond_t *opus_sender_cond;
};

int ra_server(int argc, char **argv);

#endif
