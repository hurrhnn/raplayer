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
#include <sys/termios.h>
#include <unistd.h>
#include <time.h>

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
    printf("Usage: %s <Running Mode>\n\n", argv[0]);
    puts("--client: Running on client mode.");
    puts("--server: Running on server mode.");
    puts("");
}

static int *status = 0;

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

struct termios orig_termios;

void reset_terminal_mode() {
    tcsetattr(0, TCSANOW, &orig_termios);
}

void set_conio_terminal_mode() {
    struct termios new_termios;

    /* take two copies - one for now, one for later */
    tcgetattr(0, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));

    /* register cleanup handler, and set the new terminal mode */
    atexit(&reset_terminal_mode);
    cfmakeraw(&new_termios);
    tcsetattr(0, TCSANOW, &new_termios);
}

int kbhit() {
    struct timeval timeval = {0, 0};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(1, &fds, NULL, NULL, &timeval);
}

int getch() {
    int result;
    unsigned char ch;
    if ((result = (int) read(STDIN_FILENO, &ch, sizeof(ch))) < 0) {
        return result;
    } else {
        return ch;
    }
}

void *pthread_receive_signal(void *p_pthread_signal_args) {
    void **callback_user_data_args = p_pthread_signal_args;
    int **is_client_eos = (int **) callback_user_data_args[0];
    pthread_cond_t *p_cond = (pthread_cond_t *) callback_user_data_args[1];
    pthread_mutex_t *p_mutex = (pthread_mutex_t *) callback_user_data_args[2];
    int *pthread_status = (int *) callback_user_data_args[3];

    while ((**is_client_eos) == -1 || !(**is_client_eos)) {
        pthread_mutex_lock(p_mutex);
        pthread_cond_wait(p_cond, p_mutex);
        *pthread_status = 1;

        pthread_mutex_unlock(p_mutex);
    }
    return NULL;
}

void *change_symbol(void *p_symbol_args) {
    void **callback_user_data_args = p_symbol_args;
    int **is_client_eos = (int **) callback_user_data_args[0];
    char *symbol = (char *) callback_user_data_args[1];

    int symbol_cnt = 0;
    char symbols[] = {'-', '\\', '|', '/'};
    while ((**is_client_eos) == -1 || !(**is_client_eos)) {
        *((char *) symbol) = symbols[symbol_cnt++ % DWORD];

        struct timespec timespec;
        timespec.tv_sec = 0;
        timespec.tv_nsec = 250000000;
        nanosleep(&timespec, NULL);
    }
    return NULL;
}

void *print_info(void *p_callback_user_data_args) {
    void **callback_user_data_args = p_callback_user_data_args;
    int **is_client_eos = (int **) callback_user_data_args[0];
    const double *volume = (double *) callback_user_data_args[1];
    uint64_t *sum_frame_cnt = (uint64_t *) callback_user_data_args[2];
    uint64_t *sum_frame_size = (uint64_t *) callback_user_data_args[3];

    int print_volume_remain_cnt = 0;
    int volume_status = 0;
    char symbol;

    set_conio_terminal_mode();

    void **p_symbol_args = calloc(sizeof(void *), WORD);
    p_symbol_args[0] = is_client_eos;
    p_symbol_args[1] = &symbol;

    void **p_pthread_signal_args = calloc(sizeof(void *), DWORD);
    p_pthread_signal_args[0] = is_client_eos;
    p_pthread_signal_args[1] = callback_user_data_args[4];
    p_pthread_signal_args[2] = callback_user_data_args[5];
    p_pthread_signal_args[3] = &volume_status;

    pthread_t pthread_signal_receiver;
    pthread_t symbol_changer;
    pthread_create(&pthread_signal_receiver, NULL, pthread_receive_signal, p_pthread_signal_args);
    pthread_create(&symbol_changer, NULL, change_symbol, p_symbol_args);

    while ((**is_client_eos) == -1);
    printf("Preparing socket sequence has been Successfully Completed.");
    printf("\n\rStarted Playing Opus Packets...\n\n\r");

    while (!(**is_client_eos)) {
        if (volume_status) {
            volume_status = 0;
            print_volume_remain_cnt = 20;
        }

        if (print_volume_remain_cnt > 0) {
            if (*volume >= 1)
                printf("[%c] Elapsed time: %.2lfs, Decoded frame size: %.2lfKB, Muted%*c\r", symbol,
                       (double) (*sum_frame_cnt * 20) / 1000, (double) (*sum_frame_size) / 1000, 8, ' ');
            else
                printf("[%c] Elapsed time: %.2lfs, Decoded frame size: %.2lfKB, %0.f%%%*c\r", symbol,
                       (double) (*sum_frame_cnt * 20) / 1000, (double) (*sum_frame_size) / 1000, ((1 - *volume) * 100),
                       8, ' ');
            print_volume_remain_cnt--;
        } else
            printf("[%c] Elapsed time: %.2lfs, Decoded frame size: %.2lfKB%*c\r", symbol,
                   (double) (*sum_frame_cnt * 20) / 1000, (double) (*sum_frame_size) / 1000, 8, ' ');

        fflush(stdout);

        struct timespec timespec;
        timespec.tv_sec = 0;
        timespec.tv_nsec = 50000000;
        nanosleep(&timespec, NULL);
    }

    pthread_cond_signal(callback_user_data_args[4]);
    pthread_join(pthread_signal_receiver, NULL);
    pthread_join(symbol_changer, NULL);

    printf("[*] Elapsed time: %.2lfs, Decoded frame size: %.2lfKB%*c\n\r",
           (double) (*sum_frame_cnt * 20) / 1000, (double) (*sum_frame_size) / 1000, 8, ' ');
    return EXIT_SUCCESS;
}

void *control_volume(void *p_callback_user_data_args) {
    void **callback_user_data_args = p_callback_user_data_args;
    int **is_client_eos = (int **) callback_user_data_args[0];
    double *volume = (double *) callback_user_data_args[1];
    pthread_cond_t *print_volume_cond = (pthread_cond_t *) callback_user_data_args[4];

    while ((**is_client_eos) == -1 || !(**is_client_eos)) {
        if (kbhit()) {
            switch (getch()) {
                case '\033':
                    getch(); /* skip the '[' */
                    switch (getch()) { /* the real value */
                        case 'A':
                            *volume = ((*volume - 0.01 < -0.01) ? *volume : *volume - 0.01); /* Increase volume. */
                            pthread_cond_signal(print_volume_cond);
                            break;
                        case 'B':
                            *volume = ((*volume + 0.01 > 1.01) ? *volume : *volume + 0.01); /* Decrease volume. */
                            pthread_cond_signal(print_volume_cond);
                            break;
                        default:
                            break;
                    }
                    break;

                case 0x03:
                    raise(SIGINT);
                    break;

                case 0x1A:
                    raise(SIGSTOP);
                    break;

                default:
                    break;
            }
        }
    }
    return NULL;
}

// FIXME: need to implement rhe server-side event callback handler
//void server_signal_timer(int signal) {
//    if (signal == SIGALRM) {
//        write(STDOUT_FILENO, "\nAll of client has been interrupted raplayer. Program now Exit.\n\r", 65);
//    }
//    **status == 1;
//}

void client_signal_timer(int signal) {
    if (signal == SIGALRM) {
        if (*status == -1)
            write(STDOUT_FILENO, "Error: Connection timed out.\n\r", 30);
        else {
            write(STDOUT_FILENO, "\nServer has been interrupted raplayer. Program now Exit.\n\r", 58);
        }
    }
    exit(signal);
}

void client_frame_callback(void *frame, int frame_size, void* user_args) {
    void **callback_user_data_args = user_args;
    double *volume = (double *) callback_user_data_args[1];
    uint64_t *sum_frame_cnt = (uint64_t *) callback_user_data_args[2];
    uint64_t *sum_frame_size = (uint64_t *) callback_user_data_args[3];

    alarm(2);
    int16_t *volume_frame = frame;
    for(int i = 0; i < frame_size * OPUS_AUDIO_CH; i++)
        volume_frame[i] = (int16_t) round(volume_frame[i] - (volume_frame[i] * *volume));

    Pa_WriteStream(callback_user_data_args[6], frame, frame_size);

    (*sum_frame_cnt)++;
    (*sum_frame_size) += frame_size;
}

int main(int argc, char **argv) {
    fclose(stderr); // Close stderr to avoid show alsa-lib error spamming.
    status = malloc(sizeof(void *));
    int err = Pa_Initialize();

    if (err != paNoError) {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        return EXIT_FAILURE;
    }

    if (argc < 2 ? true : !strcmp(argv[1], "--client") ? false : !strcmp(argv[1], "--server") ? false : true)
        print_usage(argv);
    else if (!strcmp(argv[1], "--client")) {
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

        *status = -1;
        signal(SIGALRM, client_signal_timer);
        PaStreamParameters outputParameters;
        PaStream *stream;

        outputParameters.device = Pa_GetDefaultOutputDevice(); /* Get default output device */
        if (outputParameters.device == paNoDevice) {
            printf("Error: No default output device.\n");
            return EXIT_FAILURE;
        }

        printf("Connecting to %s:%d..\r\n", argv[2], port);
        alarm(2);

        double volume = 0.5;
        uint64_t sum_frame_cnt = 0;
        uint64_t sum_frame_size = 0;
        pthread_cond_t print_volume_cond = PTHREAD_COND_INITIALIZER;
        pthread_mutex_t print_volume_mutex = PTHREAD_MUTEX_INITIALIZER;

        void **p_callback_user_data_args = calloc(sizeof(void *), DWORD * 2);
        p_callback_user_data_args[0] = &status;
        p_callback_user_data_args[1] = &volume;
        p_callback_user_data_args[2] = &sum_frame_cnt;
        p_callback_user_data_args[3] = &sum_frame_size;
        p_callback_user_data_args[4] = &print_volume_cond;
        p_callback_user_data_args[5] = &print_volume_mutex;

        pthread_t info_printer;
        pthread_t volume_controller;

        outputParameters.channelCount = OPUS_AUDIO_CH;
        outputParameters.sampleFormat = paInt16; /* 16 bit integer output */
        outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
        outputParameters.hostApiSpecificStreamInfo = NULL;

        Pa_OpenStream(
                &stream,
                NULL, /* no input */
                &outputParameters,
                (double) OPUS_AUDIO_SR,
                paFramesPerBufferUnspecified,
                paClipOff, /* we won't output out of range samples so don't bother clipping them */
                NULL, /* no callback, use blocking I/O */
                NULL);

        p_callback_user_data_args[6] = stream;

        Pa_StartStream(stream);
        pthread_create(&info_printer, NULL, print_info, p_callback_user_data_args); // Activate info printer.
        pthread_create(&volume_controller, NULL, control_volume, p_callback_user_data_args); // Activate volume controller.
        ra_client(argv[2], port, client_frame_callback, p_callback_user_data_args, &status);

        pthread_join(info_printer, NULL);
        pthread_join(volume_controller, NULL);

        Pa_StopStream(stream);
        Pa_CloseStream(stream);
        Pa_Terminate();
    }
    else if (!strcmp(argv[1], "--server")) {
        bool pipe_mode = false;
        bool stream_mode = false;

        // TODO: Deprecate the --stream arg, raplayer libraries will detect the stream mode automatically.
        if (argc < 3 || (strcmp(argv[2], "help") == 0)) {
            puts("");
            printf("Usage: %s --server [--stream] <FILE> [Port]\n\n", argv[0]);
            puts("<FILE>: The name of the pcm_s16le wav file to play. (\"-\" to receive from STDIN)");
            puts("[--stream]: Allow consume streams from STDIN until client connected. (DEPRECATED)");
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
            fin_name = "STREAM";
            pipe_mode = true;
        }

        if (!pipe_mode && stream_mode) {
            free(pcm_struct);
            fprintf(stdout, "Invalid argument: --stream argument cannot run without <file> argument \"-\".\n");
            return EXIT_FAILURE;
        }

        FILE *fin;
        fin = (pipe_mode ? stdin : fopen(fin_name, "rb"));

        if (fin == NULL) {
            free(pcm_struct);
            fprintf(stdout, "Error: Failed to open input file: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }

        fpos_t before_data_pos;
        if (!pipe_mode)
            init_pcm_structure(fin, pcm_struct, &before_data_pos);

        if (!pipe_mode && (pcm_struct->pcmFmtChunk.channels != 2 ||
                           pcm_struct->pcmFmtChunk.sample_rate != 48000 ||
                           pcm_struct->pcmFmtChunk.bits_per_sample != 16)) {
            free(pcm_struct);
            fprintf(stdout, "Error: Failed to open input file - must be a pcm_s16le 48000hz 2 channels wav file.\n");
            return EXIT_FAILURE;
        }

        printf("\nFile %s info: \n", fin_name);
        printf("Channels: %hd\n", pcm_struct->pcmFmtChunk.channels);
        printf("Sample rate: %u\n", pcm_struct->pcmFmtChunk.sample_rate);
        printf("Bit per sample: %hd\n", pcm_struct->pcmFmtChunk.bits_per_sample);
        if (pipe_mode)
            printf("PCM data length: STREAM\n\n");
        else
            printf("PCM data length: %u\n\n", pcm_struct->pcmDataChunk.chunk_size);

        puts("Waiting for Client... ");
        fflush(stdout);

        if (!pipe_mode)
            fsetpos(fin, &before_data_pos); // Re-read pcm data bytes from stream.

//        signal(SIGALRM, &server_signal_timer);
        ra_server(port, fileno(fin), pcm_struct->pcmDataChunk.chunk_size, &status);
    }
    return EXIT_SUCCESS;
}
