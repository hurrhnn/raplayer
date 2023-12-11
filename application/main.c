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

#include <raplayer.h>
#include <portaudio.h>
#include <termios.h>
#include <math.h>
#include <sys/errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <getopt.h>

#define OPTIONAL_ARGUMENT_IS_PRESENT \
    ((optarg == NULL && optind < argc && argv[optind][0] != '-') \
     ? (bool) (optarg = argv[optind++]) \
     : (optarg != NULL))

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

void print_usage(char **argv) {
    puts("");
    printf("Usage: ./%s -l|-c -a [address] -p [Port] -f <FILE>\n\n",
           (strrchr(argv[0], '/') ? strrchr(argv[0], '/') + 1 : argv[0]));
    puts("-l|-c: Run application with <host/peer> mode.");
    puts("-a: The IP or address to which you want to bind or connect.");
    puts("-p: The port number which you want to open or connect.");
    puts("-f: The name of the pcm_s16le wav file to play. (\"-\" to receive stream from STDIN)");
    puts("");
    exit(EXIT_SUCCESS);
}

void init_pcm_structure(FILE *fin, struct pcm *pPcm) {
    fread(pPcm->pcmHeader.chunk_id, RA_DWORD, 1, fin);
    fread(&pPcm->pcmHeader.chunk_size, RA_DWORD, 1, fin);
    fread(pPcm->pcmHeader.format, RA_DWORD, 1, fin);

    fread(pPcm->pcmFmtChunk.chunk_id, RA_DWORD, 1, fin);
    fread(&pPcm->pcmFmtChunk.chunk_size, RA_DWORD, 1, fin);
    fread(&pPcm->pcmFmtChunk.audio_format, RA_WORD, 1, fin);
    fread(&pPcm->pcmFmtChunk.channels, RA_WORD, 1, fin);
    fread(&pPcm->pcmFmtChunk.sample_rate, RA_DWORD, 1, fin);
    fread(&pPcm->pcmFmtChunk.byte_rate, RA_DWORD, 1, fin);
    fread(&pPcm->pcmFmtChunk.block_align, RA_WORD, 1, fin);
    fread(&pPcm->pcmFmtChunk.bits_per_sample, RA_WORD, 1, fin);

    char tmpBytes[RA_BYTE] = {};
    while (1) {
        if (feof(fin)) { // End Of File.
            printf("Error: The data chunk of the file not found.\n");
            exit(EXIT_FAILURE);
        }

        if (fread(tmpBytes, RA_BYTE, 1, fin) && tmpBytes[0] == 'd')
            if (fread(tmpBytes, RA_BYTE, 1, fin) && tmpBytes[0] == 'a')
                if (fread(tmpBytes, RA_BYTE, 1, fin) && tmpBytes[0] == 't')
                    if (fread(tmpBytes, RA_BYTE, 1, fin) && tmpBytes[0] == 'a')  // A PCM *d a t a* signature.
                        break;
    }

    memcpy(pPcm->pcmDataChunk.chunk_id, "data", RA_DWORD);
    fread(&pPcm->pcmDataChunk.chunk_size, RA_DWORD, 1, fin);
}

//struct termios orig_termios;
//
//void reset_terminal_mode() {
//    tcsetattr(0, TCSANOW, &orig_termios);
//}
//
//void set_conio_terminal_mode() {
//    struct termios new_termios;
//
//    /* take two copies - one for now, one for later */
//    tcgetattr(0, &orig_termios);
//    memcpy(&new_termios, &orig_termios, sizeof(new_termios));
//
//    /* register cleanup handler, and set the new terminal mode */
//    atexit(&reset_terminal_mode);
//    cfmakeraw(&new_termios);
//    tcsetattr(0, TCSANOW, &new_termios);
//}
//
//int kbhit() {
//    struct timeval timeval = {0, 0};
//    fd_set fds;
//    FD_ZERO(&fds);
//    FD_SET(STDIN_FILENO, &fds);
//    return select(1, &fds, NULL, NULL, &timeval);
//}
//
//int getch() {
//    int result;
//    unsigned char ch;
//    if ((result = (int) read(STDIN_FILENO, &ch, sizeof(ch))) < 0) {
//        return result;
//    } else {
//        return ch;
//    }
//}
//
//void *pthread_receive_signal(void *p_pthread_signal_args) {
//    void **callback_user_data_args = p_pthread_signal_args;
//    pthread_cond_t *p_cond = (pthread_cond_t *) callback_user_data_args[1];
//    pthread_mutex_t *p_mutex = (pthread_mutex_t *) callback_user_data_args[2];
//    int *pthread_status = (int *) callback_user_data_args[3];
//
//    while (!raplayer_get_client_status(callback_user_data_args[0], 0, RA_NODE_CONNECTION_EXHAUSTED)) {
//        pthread_mutex_lock(p_mutex);
//        pthread_cond_wait(p_cond, p_mutex);
//        *pthread_status = 1;
//
//        pthread_mutex_unlock(p_mutex);
//    }
//    return NULL;
//}
//
//void *change_symbol(void *p_symbol_args) {
//    void **callback_user_data_args = p_symbol_args;
//    char *symbol = (char *) callback_user_data_args[1];
//
//    int symbol_cnt = 0;
//    char symbols[] = {'-', '\\', '|', '/'};
//    while (!raplayer_get_client_status(callback_user_data_args[0], 0, RA_NODE_CONNECTION_EXHAUSTED)) {
//        *((char *) symbol) = symbols[symbol_cnt++ % RA_DWORD];
//
//        struct timespec timespec;
//        timespec.tv_sec = 0;
//        timespec.tv_nsec = 250000000;
//        nanosleep(&timespec, NULL);
//    }
//    return NULL;
//}
//
//void *print_info(void *p_callback_user_data_args) {
//    void **callback_user_data_args = p_callback_user_data_args;
//    const double *volume = (double *) callback_user_data_args[1];
//    uint64_t *sum_frame_cnt = (uint64_t *) callback_user_data_args[2];
//    uint64_t *sum_frame_size = (uint64_t *) callback_user_data_args[3];
//
//    int print_volume_remain_cnt = 0;
//    int volume_status = 0;
//    char symbol;
//
//    set_conio_terminal_mode();
//
//    void **p_symbol_args = calloc(sizeof(void *), RA_WORD);
//    p_symbol_args[0] = callback_user_data_args[0];
//    p_symbol_args[1] = &symbol;
//
//    void **p_pthread_signal_args = calloc(sizeof(void *), RA_DWORD);
//    p_pthread_signal_args[0] = callback_user_data_args[0];
//    p_pthread_signal_args[1] = callback_user_data_args[4];
//    p_pthread_signal_args[2] = callback_user_data_args[5];
//    p_pthread_signal_args[3] = &volume_status;
//
//    pthread_t pthread_signal_receiver;
//    pthread_t symbol_changer;
//    pthread_create(&pthread_signal_receiver, NULL, pthread_receive_signal, p_pthread_signal_args);
//    pthread_create(&symbol_changer, NULL, change_symbol, p_symbol_args);
//
//    while (!raplayer_get_client_status(callback_user_data_args[0], 0, RA_NODE_CONNECTED));
//    printf("Preparing socket sequence has been Successfully Completed.");
//    printf("\n\rStarted Playing Opus Packets...\n\n\r");
//
//    while (!(raplayer_get_client_status(callback_user_data_args[0], 0, RA_NODE_CONNECTION_EXHAUSTED))) {
//        if (volume_status) {
//            volume_status = 0;
//            print_volume_remain_cnt = 20;
//        }
//
//        if (print_volume_remain_cnt > 0) {
//            if (*volume >= 1)
//                printf("[%c] Elapsed time: %.2lfs, Decoded frame size: %.2lfKB, Muted%*c\r", symbol,
//                       (double) (*sum_frame_cnt * 20) / 1000, (double) (*sum_frame_size) / 1000, 8, ' ');
//            else
//                printf("[%c] Elapsed time: %.2lfs, Decoded frame size: %.2lfKB, %0.f%%%*c\r", symbol,
//                       (double) (*sum_frame_cnt * 20) / 1000, (double) (*sum_frame_size) / 1000, ((1 - *volume) * 100),
//                       8, ' ');
//            print_volume_remain_cnt--;
//        } else
//            printf("[%c] Elapsed time: %.2lfs, Decoded frame size: %.2lfKB%*c\r", symbol,
//                   (double) (*sum_frame_cnt * 20) / 1000, (double) (*sum_frame_size) / 1000, 8, ' ');
//
//        fflush(stdout);
//
//        struct timespec timespec;
//        timespec.tv_sec = 0;
//        timespec.tv_nsec = 50000000;
//        nanosleep(&timespec, NULL);
//    }
//
//    pthread_cond_signal(callback_user_data_args[4]);
//    pthread_join(pthread_signal_receiver, NULL);
//    pthread_join(symbol_changer, NULL);
//
//    printf("[*] Elapsed time: %.2lfs, Decoded frame size: %.2lfKB%*c\n\r",
//           (double) (*sum_frame_cnt * 20) / 1000, (double) (*sum_frame_size) / 1000, 8, ' ');
//    return EXIT_SUCCESS;
//}
//
//void *control_volume(void *p_callback_user_data_args) {
//    void **callback_user_data_args = p_callback_user_data_args;
//    double *volume = (double *) callback_user_data_args[1];
//    pthread_cond_t *print_volume_cond = (pthread_cond_t *) callback_user_data_args[4];
//
//    while (!raplayer_get_client_status(callback_user_data_args[0], 0, RA_NODE_CONNECTION_EXHAUSTED)) {
//        if (kbhit()) {
//            switch (getch()) {
//                case '\033':
//                    getch(); /* skip the '[' */
//                    switch (getch()) { /* the real value */
//                        case 'A':
//                            *volume = ((*volume - 0.01 < -0.01) ? *volume : *volume - 0.01); /* Increase volume. */
//                            pthread_cond_signal(print_volume_cond);
//                            break;
//                        case 'B':
//                            *volume = ((*volume + 0.01 > 1.01) ? *volume : *volume + 0.01); /* Decrease volume. */
//                            pthread_cond_signal(print_volume_cond);
//                            break;
//                        default:
//                            break;
//                    }
//                    break;
//
//                case 0x03:
//                case 0x1A:
//                    raplayer_set_client_status(callback_user_data_args[0], 0, RA_NODE_CONNECTION_EXHAUSTED);
//                    break;
//
//                default:
//                    break;
//            }
//        }
//    }
//    return NULL;
//}

void receive_frame_callback(void *frame, int frame_size, void *user_args) {
    void **callback_user_data_args = user_args;
    double *volume = (double *) callback_user_data_args[1];
    uint64_t *sum_frame_cnt = (uint64_t *) callback_user_data_args[2];
    uint64_t *sum_frame_size = (uint64_t *) callback_user_data_args[3];

    int16_t *volume_frame = frame;
    for (int i = 0; i < frame_size * RA_OPUS_AUDIO_CH; i++)
        volume_frame[i] = (int16_t) round(volume_frame[i] - (volume_frame[i] * *volume));

    Pa_WriteStream(callback_user_data_args[6], frame, frame_size);

    (*sum_frame_cnt)++;
    (*sum_frame_size) += frame_size * RA_WORD * RA_OPUS_AUDIO_CH;
}

void *provide_frame_callback(void *user_args) {
    void **callback_user_data_args = user_args;
    fread((void *) callback_user_data_args[2], RA_WORD * RA_OPUS_AUDIO_CH, RA_FRAME_SIZE,
          (void *) callback_user_data_args[1]);
    return callback_user_data_args[2];
}

int main(int argc, char **argv) {
    raplayer_t raplayer;
    raplayer_init_context(&raplayer);

    int port = 3845;
    char *address = NULL, *file_name = NULL, *program_name = argv[0];
    bool pipe_mode = false;
    bool operate_mode = false;

    int opt;
    struct option options[] = {
            {"help", no_argument,       0, 'h'},
            {"host", no_argument,       0, 'l'},
            {"peer", no_argument,       0, 'c'},
            {"addr", required_argument, 0, 'a'},
            {"port", required_argument, 0, 'p'},
            {"file", required_argument, 0, 'f'},
    };

    do {
        int cur_ind = optind;
        opt = getopt_long(argc, argv, "hlca:p:f:", options, NULL);
        switch (opt) {
            case 'h':
                print_usage(argv);
                break;
            case 'l':
                operate_mode = false;
                break;
            case 'c':
                operate_mode = true;
                break;
            case 'a':
                address = strdup(optarg);
                break;
            case 'p':
                port = (int) strtol(optarg, NULL, 10);
                break;
            case 'f':
                file_name = strdup(optarg);
                break;
            case '?':
                printf("%s: unrecognized option '%s'", program_name, argv[cur_ind]);
                print_usage(argv);
                break;
            case -1:
            default:
                break;
        }
    } while (opt != -1);

    if(file_name == NULL)
    {
        printf("Error: File name or pipe indicator must be specified.\n");
        print_usage(argv);
    }

    if(operate_mode && address == NULL) {
        printf("Error: remote address must be specified when peer mode.\n");
        print_usage(argv);
    }

    fclose(stderr); // Close stderr to avoid show alsa-lib error spamming.
    int err = Pa_Initialize();

    if (err != paNoError) {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        return EXIT_FAILURE;
    }

    PaStreamParameters outputParameters;
    PaStream *stream;

    outputParameters.device = Pa_GetDefaultOutputDevice(); /* Get default output device */
    if (outputParameters.device == paNoDevice) {
        printf("Error: No default output device.\n");
        return EXIT_FAILURE;
    }

    struct pcm *pcm_struct = calloc(sizeof(struct pcm), RA_BYTE);

    if (strcmp(file_name, "-") == 0) {
        pcm_struct->pcmFmtChunk.channels = 2;
        pcm_struct->pcmFmtChunk.sample_rate = 48000;
        pcm_struct->pcmFmtChunk.bits_per_sample = 16;
        pcm_struct->pcmDataChunk.chunk_size = 0;
        file_name = "STREAM";
        pipe_mode = true;
    }

    FILE *fin;
    fin = (pipe_mode ? stdin : fopen(file_name, "rb"));

    if (fin == NULL) {
        free(pcm_struct);
        fprintf(stdout, "Error: Failed to open input file: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    if (!pipe_mode)
        init_pcm_structure(fin, pcm_struct);

    if (!pipe_mode && (pcm_struct->pcmFmtChunk.channels != 2 ||
                       pcm_struct->pcmFmtChunk.sample_rate != 48000 ||
                       pcm_struct->pcmFmtChunk.bits_per_sample != 16)) {
        free(pcm_struct);
        fprintf(stdout, "Error: Failed to open input file - must be a pcm_s16le 48000hz 2 channels wav file.\n");
        return EXIT_FAILURE;
    }

    /* Set fd to non-blocking mode. */
    int flags = fcntl(fileno(fin), F_GETFL, 0);
    fcntl(fileno(fin), F_SETFL, flags | O_NONBLOCK);

    printf("\nFile %s info: \n", file_name);
    printf("Channels: %hd\n", pcm_struct->pcmFmtChunk.channels);
    printf("Sample rate: %u\n", pcm_struct->pcmFmtChunk.sample_rate);
    printf("Bit per sample: %hd\n", pcm_struct->pcmFmtChunk.bits_per_sample);
    if (pipe_mode)
        printf("PCM data length: STREAM\n\n");
    else
        printf("PCM data length: %u\n\n", pcm_struct->pcmDataChunk.chunk_size);
    fflush(stdout);

    if (operate_mode)
        printf("Connecting to %s:%d..\r\n", address, port);

    double volume = 0.5;
    uint64_t sum_frame_cnt = 0;
    uint64_t sum_frame_size = 0;
    pthread_cond_t print_volume_cond = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t print_volume_mutex = PTHREAD_MUTEX_INITIALIZER;

    void **p_send_cb_user_data_args = calloc(sizeof(void *), RA_DWORD);
    p_send_cb_user_data_args[0] = &raplayer;
    p_send_cb_user_data_args[1] = fin;
    p_send_cb_user_data_args[2] = malloc(RA_FRAME_SIZE * RA_OPUS_AUDIO_CH * RA_WORD);

    void **p_recv_cb_user_data_args = calloc(sizeof(void *), RA_DWORD * 2);
    p_recv_cb_user_data_args[0] = &raplayer;
    p_recv_cb_user_data_args[1] = &volume;
    p_recv_cb_user_data_args[2] = &sum_frame_cnt;
    p_recv_cb_user_data_args[3] = &sum_frame_size;
    p_recv_cb_user_data_args[4] = &print_volume_cond;
    p_recv_cb_user_data_args[5] = &print_volume_mutex;

    outputParameters.channelCount = RA_OPUS_AUDIO_CH;
    outputParameters.sampleFormat = paInt16; /* 16 bit integer output */
    outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

    Pa_OpenStream(
            &stream,
            NULL, /* no input */
            &outputParameters,
            (double) RA_OPUS_AUDIO_SR,
            paFramesPerBufferUnspecified,
            paClipOff, /* we won't output out of range samples so don't bother clipping them */
            NULL, /* no callback, use blocking I/O */
            NULL);

    p_recv_cb_user_data_args[6] = stream;
    Pa_StartStream(stream);

    raplayer_spawn(&raplayer, operate_mode, address, port);
    int64_t provider_media_id =
            raplayer_register_media_provider(&raplayer, provide_frame_callback, p_send_cb_user_data_args);

    int64_t consumer_media_id =
            raplayer_register_media_consumer(&raplayer, receive_frame_callback, p_recv_cb_user_data_args);

    while (true);

//    uint64_t id = raplayer_spawn_server(&raplayer, port, provide_frame_callback, p_recv_cb_user_data_args);
//    int32_t status = raplayer_wait_server(&raplayer, id);
//    if (status != 0)
//        printf("Server stopped with exit code %d, %s.\n", status, raplayer_strerror(status));
//
//    pthread_t info_printer;
//    pthread_t volume_controller;
//
//    pthread_create(&info_printer, NULL, print_info, p_recv_cb_user_data_args); // Activate info printer.
//    pthread_create(&volume_controller, NULL, control_volume,
//                   p_recv_cb_user_data_args); // Activate volume controller.
//
//    int32_t status = raplayer_wait_client(&raplayer, id);
//    if (status != 0)
//        printf("Client finished with exit code %d, %s.\n", status, raplayer_strerror(status));

    free(p_recv_cb_user_data_args[2]);
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    Pa_Terminate();
    return EXIT_SUCCESS;
}
