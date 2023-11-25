#include <raplayer/utils.h>

uint64_t provide_20ms_opus_offset_calculator(unsigned char c_bits[MAX_DATA_SIZE], unsigned char **result) {
    long unsigned int idx = 0;
    for (long unsigned int i = 0; i < MAX_DATA_SIZE; i++) { // 'OPUS' indicates for Start of opus stream.
        if (c_bits[i] == 'O' && c_bits[i + 1] == 'P' && c_bits[i + 2] == 'U' && c_bits[i + 3] == 'S') {
            idx = i - 1;
            break;
        }
    }

    char str_nbBytes[DWORD];
    for (long unsigned int i = 0; i <= idx + 1; i++) {
        if (i == (idx + 1))
            str_nbBytes[i] = '\0';
        else
            str_nbBytes[i] = (char) c_bits[i];
    }
    *result = c_bits + idx + sizeof(OPUS_FLAG);
    return strtol(str_nbBytes, NULL, 10);
}