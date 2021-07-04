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

#include "ra_client.h"
#include "ra_server.h"

void print_usage(char **argv) {
    puts("");
    printf("Usage: %s <Running Mode>\n\n", argv[0]);
    puts("--client: Running on client mode.");
    puts("--server: Running on server mode.");
    puts("");
}

int main(int argc, char **argv) {
    fclose(stderr); // Close stderr to avoid showing logging in the alsa-lib.
    int err = Pa_Initialize();

    if (err != paNoError) {
        printf("PortAudio error: %s\n", Pa_GetErrorText(err));
        return EXIT_FAILURE;
    }

    if (argc < 2 ? true : !strcmp(argv[1], "--client") ? false : !strcmp(argv[1], "--server") ? false : true)
        print_usage(argv);
    else if (!strcmp(argv[1], "--client"))
        ra_client(argc, argv);
    else if (!strcmp(argv[1], "--server"))
        ra_server(argc, argv);

    return EXIT_SUCCESS;
}
