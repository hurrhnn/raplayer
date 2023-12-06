#include <raplayer/utils.h>

const char *raplayer_strerror(int err) {
    switch (err) {
        case RAPLAYER_SOCKET_CREATION_FAILED:
            return "Socket Creation Failed";
        case RA_CLIENT_CONNECTION_RESOLVE_FAILED:
            return "Connection Cannot resolved to address";
        case RA_CLIENT_ADDRESS_CONVERSION_FAILED:
            return "Convert internet host address Failed";
        case RA_CLIENT_SOCKET_INIT_SEQ1_FAILED:
            return "Client socket initiating sequence 1 Failed";
        case RA_CLIENT_SOCKET_INIT_SEQ2_FAILED:
            return "Client socket initiating sequence 2 Failed";
        case RA_CLIENT_CREATE_OPUS_DECODER_FAILED:
            return "Failed to create opus decoder";
        case RA_CLIENT_OPUS_DECODE_FAILED:
            return "Opus decoder failed to operate";
        case RA_SERVER_SOCKET_BIND_FAILED:
            return "Socket bind Failed";
        case RA_SERVER_SOCKET_INIT_SEQ1_FAILED:
            return "Incoming connection socket initiating sequence 1 Failed";
        case RA_SERVER_SOCKET_INIT_SEQ2_FAILED:
            return "Incoming connection socket initiating sequence 2 Failed";
        case RA_SERVER_SOCKET_INIT_SEQ3_FAILED:
            return "Incoming connection socket initiating sequence 3 Failed";
        case RA_SERVER_CREATE_OPUS_ENCODER_FAILED:
            return "Failed to create opus encoder";
        case RA_SERVER_OPUS_ENCODER_CTL_FAILED:
            return "Failed to setting opus encode bitrate";
        case RA_SERVER_OPUS_ENCODE_FAILED:
            return "Opus encoder failed to operate";
        default:
            return "Unknown error occurred";
    }
}

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