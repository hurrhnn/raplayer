#ifndef RAPLAYER_UTILS_H
#define RAPLAYER_UTILS_H

#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <raplayer/config.h>

#include <stdio.h>

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

#define STR_RST   "\x1B[0m"
#define STR_RED   "\x1B[31m"
#define STR_GRN   "\x1B[32m"
#define STR_YEL   "\x1B[33m"
#define STR_BLU   "\x1B[34m"
#define STR_MAG   "\x1B[35m"
#define STR_CYN   "\x1B[36m"
#define STR_WHT   "\x1B[37m"

typedef enum {
    RED = 0,
    GRN,
    YEL,
    BLU,
    MAG,
    CYN,
    WHT,
    NO_COLOR,
} ra_debug_color_t;

#define RA_DEBUG_COLOR(ENUM) \
    (((ENUM) == RED) ? STR_RED :   \
     ((ENUM) == GRN) ? STR_GRN :   \
     ((ENUM) == YEL) ? STR_YEL :   \
     ((ENUM) == BLU) ? STR_BLU :   \
     ((ENUM) == MAG) ? STR_MAG :   \
     ((ENUM) == CYN) ? STR_CYN :   \
     ((ENUM) == WHT) ? STR_WHT : "" )

#define RA_LOGGER_PRINTF(fmt, ...) \
do {                                  \
    printf(fmt, ##__VA_ARGS__);       \
} while (0)

#if RAPLAYER_ENABLE_DEBUG
#define RA_DEBUG(CLR, fmt, ...) \
    RA_LOGGER_PRINTF("%s" fmt "%s", (RA_DEBUG_COLOR(CLR)), ##__VA_ARGS__, STR_RST)
#define RA_DEBUG_MORE(CLR, fmt, ...) \
    RA_LOGGER_PRINTF("%s[%s:%02d]: %s() - " fmt "%s", (RA_DEBUG_COLOR(CLR)), __FILE_NAME__, __LINE__, __func__, ##__VA_ARGS__, STR_RST)

#define RA_INFO(fmt, ...) \
    RA_DEBUG_MORE(NO_COLOR, fmt, ##__VA_ARGS__)
#define RA_WARN(fmt, ...) \
    RA_DEBUG_MORE(YEL, fmt, ##__VA_ARGS__)
#define RA_ERROR(fmt, ...) \
    RA_DEBUG_MORE(RED, fmt, ##__VA_ARGS__)
#else
#define RA_DEBUG(CLR, fmt, ...) do {} while (0)
#define RA_DEBUG_MORE(CLR, fmt, ...) do {} while (0)

#define RA_INFO(fmt, ...) \
    RA_LOGGER_PRINTF(##__VA_ARGS__)
#define RA_WARN(fmt, ...) \
    RA_LOGGER_PRINTF("%s" fmt "%s", STR_YEL, ##__VA_ARGS__, STR_RST)
#define RA_ERROR(fmt, ...) \
    RA_LOGGER_PRINTF("%s" fmt "%s", STR_RED, ##__VA_ARGS__, STR_RST)
#endif

const char *raplayer_strerror(int err);

uint64_t provide_20ms_opus_offset_calculator(unsigned char c_bits[MAX_DATA_SIZE], unsigned char **result);

#endif //RAPLAYER_UTILS_H
