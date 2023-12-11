#include "raplayer/utils.h"

const char *raplayer_strerror(int err) {
    switch (err) {
        case RA_SOCKET_CREATION_FAILED:
            return "Socket creation failed";
        case RA_CONNECTION_RESOLVE_FAILED:
            return "Connection cannot resolved to address";
        case RA_ADDRESS_CONVERSION_FAILED:
            return "Convert internet host address failed";
        case RA_SOCKET_INIT_SEQ1_FAILED:
            return "Socket initiating sequence 1 failed";
        case RA_SOCKET_INIT_SEQ2_FAILED:
            return "Socket initiating sequence 2 failed";
        case RA_SOCKET_INIT_SEQ3_FAILED:
            return "Socket initiating sequence 3 failed";
        case RA_CREATE_OPUS_DECODER_FAILED:
            return "Failed to create opus decoder";
        case RA_OPUS_DECODE_FAILED:
            return "Opus decoder failed to operate";
        case RA_SOCKET_BIND_FAILED:
            return "Socket bind failed";
        case RA_CREATE_OPUS_ENCODER_FAILED:
            return "Failed to create opus encoder";
        case RA_OPUS_ENCODER_CTL_FAILED:
            return "Failed to setting opus encode bitrate";
        case RA_OPUS_ENCODE_FAILED:
            return "Opus encoder failed to operate";
        case RA_MEDIA_INVALID_ARGUMENT:
            return "Media arguments are invalid";
        default:
            return "Unknown error occurred";
    }
}

bool ra_compare_sockaddr(struct sockaddr_in *s1, struct sockaddr_in *s2) {
    return ((s1->sin_addr.s_addr == s2->sin_addr.s_addr) && (s1->sin_port == s2->sin_port));
}

u_int16_t ra_swap_endian_uint16(u_int16_t number) {
    return (number >> 8) | (number << 8);
}

u_int32_t ra_swap_endian_uint32(u_int32_t number) {
    return ((number >> 24) & 0xFF) |
           ((number << 8) & 0xFF0000) |
           ((number >> 8) & 0xFF00) |
           ((number << 24) & 0xFF000000);
}