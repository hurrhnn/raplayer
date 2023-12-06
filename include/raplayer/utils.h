#ifndef RAPLAYER_UTILS_H
#define RAPLAYER_UTILS_H

#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <raplayer/config.h>

#ifdef __ANDROID__
#ifndef RAPLAYER_ANDROID_WORKAROUND
#define RAPLAYER_ANDROID_WORKAROUND

#include <pthread.h>
#define pthread_cancel(h) pthread_kill(h, 0)

#define PTHREAD_CANCEL_ENABLE 0
#define PTHREAD_CANCEL_ASYNCHRONOUS 0
#define pthread_setcanceltype(foo, bar) \
        while(foo);                     \
        while(bar);                     \

#define pthread_setcancelstate(foo, bar) pthread_setcanceltype(foo, bar)
#endif
#endif /* __ANDROID__ */

typedef enum {
    RAPLAYER_SOCKET_CREATION_FAILED = 1,
    RA_CLIENT_CONNECTION_RESOLVE_FAILED,
    RA_CLIENT_ADDRESS_CONVERSION_FAILED,
    RA_CLIENT_SOCKET_INIT_SEQ1_FAILED,
    RA_CLIENT_SOCKET_INIT_SEQ2_FAILED,
    RA_CLIENT_CREATE_OPUS_DECODER_FAILED,
    RA_CLIENT_OPUS_DECODE_FAILED,
    RA_SERVER_SOCKET_BIND_FAILED,
    RA_SERVER_SOCKET_INIT_SEQ1_FAILED,
    RA_SERVER_SOCKET_INIT_SEQ2_FAILED,
    RA_SERVER_SOCKET_INIT_SEQ3_FAILED,
    RA_SERVER_CREATE_OPUS_ENCODER_FAILED,
    RA_SERVER_OPUS_ENCODER_CTL_FAILED,
    RA_SERVER_OPUS_ENCODE_FAILED,
} raplayer_errno_t;

typedef enum {
    RA_NODE_INITIATED = (1 << 0),
    RA_NODE_CONNECTED = (1 << 1),
    RA_NODE_CONNECTION_EXHAUSTED = (1 << 2),
    RA_NODE_HEARTBEAT_RECEIVED = (1 << 3),
} ra_node_status_t;

const char *raplayer_strerror(int err);

uint64_t provide_20ms_opus_offset_calculator(unsigned char c_bits[MAX_DATA_SIZE], unsigned char **result);

#endif //RAPLAYER_UTILS_H
