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
#include "ra_server.h"
#include "chacha20/chacha20.h"
#include "task_scheduler/task_scheduler.h"
#include "task_dispatcher/task_dispatcher.h"

bool is_EOS = false;

void cleanup(int argc, ...) {
    va_list args;
    va_start(args, argc);
    for (int i = 0; i < argc; i++)
        free(va_arg(args, void *));
    va_end(args);

}

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

    char tmpBytes[BYTE] = {};
    while (1) {
        if (feof(fin)) { // End Of File.
            printf("Error: The data chunk of the file not found.\n");
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
    pPcm->pcmDataChunk.data = malloc(pcm_data_size);
    fread(pPcm->pcmDataChunk.data, pcm_data_size, 1, fin);
}

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
    if (strncmp(task.buffer, "HELLO", task.buffer_len) == 0)
        return true;
    else
        return false;
}

bool ready_sock_server_seq2(TaskQueue *recv_queue, struct pcm pcm_struct) {

    int stream_info_size = DWORD + WORD * 2;
    int buffer_size = 5;

    char stream_info[stream_info_size];
    memcpy(stream_info, &pcm_struct.pcmFmtChunk.channels, WORD);
    memcpy((stream_info + WORD), &pcm_struct.pcmFmtChunk.sample_rate, DWORD);
    memcpy((stream_info + WORD + DWORD), &pcm_struct.pcmFmtChunk.bits_per_sample, WORD);

    char *buffer = calloc(buffer_size, BYTE);

    sprintf(buffer, "%d", stream_info_size);
    sendto(recv_queue->queue_info->sock_fd, buffer, buffer_size, 0,
           (struct sockaddr *) &recv_queue->queue_info->client->client_addr,
           recv_queue->queue_info->client->socket_len);
    cleanup(1, buffer);

    Task task = recvfrom_queue(recv_queue);
    if (strncmp(task.buffer, "OK", task.buffer_len) != 0) {
        return 0;
    }

    if (stream_info_size ==
        sendto(recv_queue->queue_info->sock_fd, stream_info, stream_info_size, 0,
               (struct sockaddr *) &recv_queue->queue_info->client->client_addr,
               recv_queue->queue_info->client->socket_len)) {
        task = recvfrom_queue(recv_queue);
        if (stream_info_size == strtol(task.buffer, NULL, 10)) {
            sendto(recv_queue->queue_info->sock_fd, &pcm_struct.pcmDataChunk.chunk_size, DWORD, 0,
                   (struct sockaddr *) &recv_queue->queue_info->client->client_addr,
                   recv_queue->queue_info->client->socket_len);

            task = recvfrom_queue(recv_queue);
            if (strncmp(task.buffer, "OK", task.buffer_len) == 0)
                return true;
        }
    }
    return false;
}

bool
ready_sock_server_seq3(TaskQueue *recv_queue, const unsigned char *crypto_payload) {
    const int crypto_payload_size = CHACHA20_NONCEBYTES + CHACHA20_KEYBYTES;

    if (crypto_payload_size ==
        sendto(recv_queue->queue_info->sock_fd, crypto_payload, crypto_payload_size, 0,
               (struct sockaddr *) &recv_queue->queue_info->client->client_addr,
               recv_queue->queue_info->client->socket_len)) {

        Task task = recvfrom_queue(recv_queue);
        if (!strncmp(task.buffer, OK, task.buffer_len))
            return true;
    }
    return false;
}


void *provide_20ms_opus_builder(void *p_opus_builder_args) {
    struct opus_builder_args *opus_builder_args = (struct opus_builder_args *) p_opus_builder_args;

    opus_int16 in[FRAME_SIZE * opus_builder_args->pcm_struct->pcmFmtChunk.channels];
    unsigned char c_bits[FRAME_SIZE];
    struct chacha20_context ctx;

    while (1) {
        unsigned char pcm_bytes[FRAME_SIZE * opus_builder_args->pcm_struct->pcmFmtChunk.channels * WORD];

        /* Read a 16 bits/sample audio frame. */
        fread(pcm_bytes, WORD * opus_builder_args->pcm_struct->pcmFmtChunk.channels, FRAME_SIZE,
              opus_builder_args->fin);
        if (feof(opus_builder_args->fin)) // End Of Stream.
            break;

        /* Convert from little-endian ordering. */
        for (int i = 0; i < opus_builder_args->pcm_struct->pcmFmtChunk.channels * FRAME_SIZE; i++)
            in[i] = (opus_int16) (pcm_bytes[2 * i + 1] << 8 | pcm_bytes[2 * i]);

        /* Encode the frame. */
        int nbBytes = opus_encode(opus_builder_args->encoder, in, FRAME_SIZE, c_bits, FRAME_SIZE);
        if (nbBytes < 0) {
            printf("Error: opus encode failed - %s\n", opus_strerror(nbBytes));
            exit(EXIT_FAILURE);
        }

        /* Encrypt the frame. */
        chacha20_init_context(&ctx, opus_builder_args->crypto_payload,
                              opus_builder_args->crypto_payload + CHACHA20_NONCEBYTES, 0);
        chacha20_xor(&ctx, c_bits, nbBytes);

        /* Create payload. */
        const unsigned int nbBytes_len = (int) floor(log10(nbBytes) + 1);
        char *buffer = calloc(nbBytes_len + sizeof(OPUS_FLAG) + nbBytes, WORD);

        sprintf(buffer, "%d", nbBytes);
        strcat(buffer, OPUS_FLAG);
        memcpy(buffer + nbBytes_len + sizeof(OPUS_FLAG) - 1, c_bits, nbBytes);

        /* Waiting for opus timer's signal & Send audio frames. */
        pthread_mutex_lock(opus_builder_args->opus_builder_mutex);
        pthread_cond_wait(opus_builder_args->opus_builder_cond, opus_builder_args->opus_builder_mutex);
        pthread_mutex_lock(opus_builder_args->opus_sender_mutex);

        memcpy(opus_builder_args->opus_frame->buffer, buffer, nbBytes_len + sizeof(OPUS_FLAG) + nbBytes);
        cleanup(1, buffer);
        opus_builder_args->opus_frame->buffer_len = (ssize_t) (nbBytes_len + sizeof(OPUS_FLAG) + nbBytes);

        pthread_mutex_unlock(opus_builder_args->opus_sender_mutex);
        pthread_cond_broadcast(opus_builder_args->opus_sender_cond);
        pthread_mutex_unlock(opus_builder_args->opus_builder_mutex);
    }
    is_EOS = true;
    return NULL;
}

void *provide_20ms_opus_sender(void *p_opus_sender_args) {
    struct opus_sender_args *opus_sender_args = (struct opus_sender_args *) p_opus_sender_args;

    while ((!is_EOS) && opus_sender_args->recv_queue->queue_info->heartbeat_status != -1) {
        pthread_mutex_lock(opus_sender_args->opus_sender_mutex);
        pthread_cond_wait(opus_sender_args->opus_sender_cond, opus_sender_args->opus_sender_mutex);
        sendto(opus_sender_args->recv_queue->queue_info->sock_fd, opus_sender_args->opus_frame->buffer,
               opus_sender_args->opus_frame->buffer_len, 0,
               (struct sockaddr *) &opus_sender_args->recv_queue->queue_info->client->client_addr,
               opus_sender_args->recv_queue->queue_info->client->socket_len);
        pthread_mutex_unlock(opus_sender_args->opus_sender_mutex);
    }
    return NULL;
}

void *provide_20ms_opus_timer(void *p_opus_builder_cond) {
    struct timespec start_timespec;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_timespec);
    time_t start_time = (start_timespec.tv_sec * 1000000000L) + start_timespec.tv_nsec, time, offset = 0L, average = 0L;

    while (!is_EOS) {
        offset += 20000000L;
        time = start_time + offset;

        struct timespec current_time, calculated_delay;
        clock_gettime(CLOCK_MONOTONIC_RAW, &current_time);
        time -= ((current_time.tv_sec * 1000000000L) + current_time.tv_nsec);

        calculated_delay.tv_sec = ((time / 1000000000L) > 0 ? (time / 1000000000L) : 0);
        calculated_delay.tv_nsec = ((time % 1000000000L) > 0 ? (time % 1000000000L) : average);

        /* Calculates the time average value for when the current time exceeds the calculated time. */
        average = (average == 0L ? ((time % 1000000000L) > 0 ? (time % 1000000000L) : average)
                : ((time % 1000000000L) > 0 ? ((time % 1000000000L) + average) / 2 : average));

        nanosleep(&calculated_delay, NULL);
        pthread_cond_signal((pthread_cond_t *) p_opus_builder_cond);

        /* Adjusts the 20ms interval if the average value was used instead. */
        if(calculated_delay.tv_nsec == average)
            average -= 250000L;
    }
    return NULL;
}

void *check_heartbeat(void *p_heartbeat_receiver_args) {
    TaskQueue *recv_queue = (TaskQueue *) p_heartbeat_receiver_args;

    struct timespec timespec;
    timespec.tv_sec = 1;
    timespec.tv_nsec = 0;

    while (!is_EOS) {
        recv_queue->queue_info->heartbeat_status = false;
        nanosleep(&timespec, NULL);
        if (!recv_queue->queue_info->heartbeat_status) {
            printf("\n%d: Connection closed by %s:%d",
                   recv_queue->queue_info->client->client_id, inet_ntoa(recv_queue->queue_info->client->client_addr.sin_addr),
                   ntohs(recv_queue->queue_info->client->client_addr.sin_port));
            printf("\nReceiving client heartbeat timed out.\n");
            fflush(stdout);
            recv_queue->queue_info->heartbeat_status = -1;
            break;
        }
    }
    return NULL;
}

_Noreturn void *handle_client(void *p_client_handler_args) {
    const int *current_clients_count = ((struct client_handler_info *) p_client_handler_args)->current_clients_count;
    const struct pcm *pcm_struct = ((struct client_handler_info *) p_client_handler_args)->pcm_struct;
    const unsigned char *crypto_payload = ((struct client_handler_info *) p_client_handler_args)->crypto_payload;

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
            if (ready_sock_server_seq2(recv_queues[(*current_clients_count) - 1], *pcm_struct)) {
                if (ready_sock_server_seq3(recv_queues[(*current_clients_count) - 1], crypto_payload))
                    printf("Preparing socket sequence has been Successfully Completed.");
                else {
                    printf("Error: A crypto preparation sequence Failed.");
                    continue;
                }
            } else {
                printf("Error: A server socket preparation sequence Failed.");
                continue;
            }
        } else {
            printf("Error: A client socket preparation sequence Failed.");
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
        p_opus_sender_args->recv_queue = recv_queues[(*current_clients_count) - 1];
        p_opus_sender_args->opus_frame = ((struct client_handler_info *) p_client_handler_args)->opus_frame;
        p_opus_sender_args->opus_sender_mutex = ((struct client_handler_info *) p_client_handler_args)->opus_sender_mutex;
        p_opus_sender_args->opus_sender_cond = ((struct client_handler_info *) p_client_handler_args)->opus_sender_cond;

        // Activate opus sender per client.
        pthread_create(&opus_sender, NULL, provide_20ms_opus_sender, (void *) p_opus_sender_args);
        pthread_create(&heartbeat_checker, NULL, check_heartbeat, (void *) recv_queues[(*current_clients_count) - 1]);
    }
}

__attribute__((noreturn)) void server_signal_timer(int signal) {
    if (signal == SIGALRM) {
        write(STDOUT_FILENO, "\nAll of client has been interrupted raplayer. Program now Exit.\n", 64);
    }
    exit(signal);
}

int ra_server(int argc, char **argv) {
    signal(SIGALRM, &server_signal_timer);

    bool pipe_mode = false;
    bool stream_mode = false;

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

    struct pcm *pcm_struct = calloc(sizeof(struct pcm), BYTE);

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
        cleanup(1, pcm_struct);
        fprintf(stdout, "Invalid argument: --stream argument cannot run without <file> argument \"-\".\n");
        return EXIT_FAILURE;
    }

    FILE *fin;
    fin = (pipe_mode ? stdin : fopen(fin_name, "rb"));

    if (fin == NULL) {
        cleanup(1, pcm_struct);
        fprintf(stdout, "Error: Failed to open input file: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    fpos_t before_data_pos;
    if (!pipe_mode)
        init_pcm_structure(fin, pcm_struct, &before_data_pos);

    if (!pipe_mode && (pcm_struct->pcmFmtChunk.channels != 2 ||
                        pcm_struct->pcmFmtChunk.sample_rate != 48000 ||
                        pcm_struct->pcmFmtChunk.bits_per_sample != 16)) {
        cleanup(1, pcm_struct);
        fprintf(stdout, "Error: Failed to open input file: It must be a pcm_s16le 48000hz 2 channels wav file.\n");
        return EXIT_FAILURE;
    }

    printf("\nFile %s info: \n", fin_name);
    printf("Channels: %hd\n", pcm_struct->pcmFmtChunk.channels);
    printf("Sample rate: %u\n", pcm_struct->pcmFmtChunk.sample_rate);
    printf("Bit per sample: %hd\n", pcm_struct->pcmFmtChunk.bits_per_sample);
    if (pipe_mode)
        printf("PCM data length: STDIN\n\n");
    else
        printf("PCM data length: %u\n\n", pcm_struct->pcmDataChunk.chunk_size);

    puts("Waiting for Client... ");
    fflush(stdout);

    if (!pipe_mode)
        fsetpos(fin, &before_data_pos); // Re-read pcm data bytes from stream.

    bool stop_consumer = false;

    pthread_t opus_timer;
    pthread_t stream_consumer;
    pthread_t *p_stream_consumer = &stream_consumer;
    pthread_mutex_t stream_consumer_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t stream_consumer_cond = PTHREAD_COND_INITIALIZER;

    if (stream_mode) {
        void **p_stream_consumer_args = calloc(sizeof(void *), DWORD);

        p_stream_consumer_args[0] = fin;
        p_stream_consumer_args[1] = &stop_consumer;
        p_stream_consumer_args[2] = &stream_consumer_mutex;
        p_stream_consumer_args[3] = &stream_consumer_cond;

        // Activate opus timer for consuming stream.
        pthread_create(&opus_timer, NULL, provide_20ms_opus_timer, (void *) &stream_consumer_cond);
        pthread_create(p_stream_consumer, NULL, consume_until_connection, (void *) p_stream_consumer_args);
    } else
        p_stream_consumer = NULL;

    struct sockaddr_in server_addr;
    int sock_fd = server_init_socket(&server_addr, port);

    //Set fd to non-blocking mode.
    int flags = fcntl(fileno(fin), F_GETFL, 0);
    fcntl(fileno(fin), F_SETFL, flags | O_NONBLOCK);

    int current_clients_count = 0;
    unsigned char *crypto_payload = generate_random_bytestream(CHACHA20_NONCEBYTES + CHACHA20_KEYBYTES);
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

    client_handler_args.current_clients_count = &current_clients_count;
    client_handler_args.recv_queues = &task_scheduler_args.recv_queues;
    client_handler_args.pcm_struct = pcm_struct;
    client_handler_args.crypto_payload = crypto_payload;

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
    encoder = opus_encoder_create((opus_int32) pcm_struct->pcmFmtChunk.sample_rate, pcm_struct->pcmFmtChunk.channels,
                                  APPLICATION,
                                  &err);
    if (err < 0) {
        printf("Error: failed to create an encoder - %s\n", opus_strerror(err));
        exit(EXIT_FAILURE);
    }

    if (opus_encoder_ctl(encoder,
                         OPUS_SET_BITRATE(pcm_struct->pcmFmtChunk.sample_rate * pcm_struct->pcmFmtChunk.channels)) <
        0) {
        printf("Error: failed to set bitrate - %s\n", opus_strerror(err));
        exit(EXIT_FAILURE);
    }

    pthread_t opus_builder;
    /* Create opus builder arguments struct. */
    struct opus_builder_args *p_opus_builder_args = malloc(sizeof(struct opus_builder_args));
    p_opus_builder_args->pcm_struct = pcm_struct;
    p_opus_builder_args->fin = fin;
    p_opus_builder_args->encoder = encoder;
    p_opus_builder_args->crypto_payload = crypto_payload;
    p_opus_builder_args->opus_frame = client_handler_args.opus_frame;
    p_opus_builder_args->opus_builder_mutex = stream_mode ? &stream_consumer_mutex : &opus_builder_mutex;
    p_opus_builder_args->opus_sender_mutex = &opus_sender_mutex;
    p_opus_builder_args->opus_builder_cond = stream_mode ? &stream_consumer_cond : &opus_builder_cond;
    p_opus_builder_args->opus_sender_cond = &opus_sender_cond;

    // Activate opus builder.
    pthread_create(&opus_builder, NULL, provide_20ms_opus_builder, (void *) p_opus_builder_args);

    if (!stream_mode)
        // Activate opus timer.
        pthread_create(&opus_timer, NULL, provide_20ms_opus_timer, (void *) &opus_builder_cond);

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
        cleanup(2, task_scheduler_args.recv_queues[i]->queue_info, task_scheduler_args.recv_queues[i]);
    }

    /* Destroy the encoder state */
    opus_encoder_destroy(encoder);

    /* Close audio stream. */
    fclose(fin);

    exit(EXIT_SUCCESS);
}
