#ifndef RAPLAYER_UTILS_H
#define RAPLAYER_UTILS_H
#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>

#define RA_MODE_HOST 0
#define RA_MODE_PEER 1

#define RA_BYTE 1
#define RA_WORD 2
#define RA_DWORD 4

#define RA_OK 0
#define RA_INVALID -1

#define RA_CTL_HEADER "\x53\x82\x37\x28"

#define RA_FRAME_SIZE 960
#define RA_MAX_DATA_SIZE 4096

#define RA_OPUS_AUDIO_CH 2
#define RA_OPUS_AUDIO_BPS 16
#define RA_OPUS_AUDIO_SR 48000
#define RA_OPUS_APPLICATION OPUS_APPLICATION_AUDIO

#define RA_MAX_QUEUE_SIZE 0x7fff

#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN || \
    defined(__BIG_ENDIAN__) || \
    defined(__ARMEB__) || \
    defined(__THUMBEB__) || \
    defined(__AARCH64EB__) || \
    defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__)
#define RA_SYSTEM_IS_BIG_ENDIAN
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN || \
    defined(__LITTLE_ENDIAN__) || \
    defined(__ARMEL__) || \
    defined(__THUMBEL__) || \
    defined(__AARCH64EL__) || \
    defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__) || \
    defined(BUFEP_MS_WINDOWS)
#define RA_SYSTEM_IS_LITTLE_ENDIAN
#else
#error "Couldn't decide the system endianness."
#endif

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

#if defined(WIN32) || defined(_WIN32) || defined(_WIN64) || defined(__WIN32) && !defined(__CYGWIN__)
#define RA_MS_WINDOWS
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#elif defined(__APPLE__) || defined(__MACH__)
#define RA_MAC_OS_X
#elif defined(__linux__)
#define RA_LINUX
#elif defined(__FreeBSD__)
#define RA_FREEBSD
#elif defined(__unix) || defined(__unix__)
#define RA_UNIX
#endif

#ifdef RA_MS_WINDOWS
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#else
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

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
    RA_LOGGER_PRINTF("%s[%s:%02d]: %s() - " fmt "%s", (RA_DEBUG_COLOR(CLR)), __FILENAME__, __LINE__, __func__, ##__VA_ARGS__, STR_RST)

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
    RA_LOGGER_PRINTF(fmt, ##__VA_ARGS__)
#define RA_WARN(fmt, ...) \
    RA_LOGGER_PRINTF("%s" fmt "%s", STR_YEL, ##__VA_ARGS__, STR_RST)
#define RA_ERROR(fmt, ...) \
    RA_LOGGER_PRINTF("%s" fmt "%s", STR_RED, ##__VA_ARGS__, STR_RST)
#endif

#define ra_realloc(addr, size) \
    if((addr = realloc(addr, size)) == NULL) { \
        RA_ERROR("realloc() return address was NULL!\n"); \
        exit(EXIT_FAILURE); \
    } \

typedef enum {
    RA_SOCKET_CREATION_FAILED = INT_MIN,
    RA_CONNECTION_RESOLVE_FAILED,
    RA_ADDRESS_CONVERSION_FAILED,
    RA_SOCKET_INIT_SEQ1_FAILED,
    RA_SOCKET_INIT_SEQ2_FAILED,
    RA_SOCKET_INIT_SEQ3_FAILED,
    RA_CREATE_OPUS_DECODER_FAILED,
    RA_OPUS_DECODE_FAILED,
    RA_SOCKET_BIND_FAILED,
    RA_CREATE_OPUS_ENCODER_FAILED,
    RA_OPUS_ENCODER_CTL_FAILED,
    RA_OPUS_ENCODE_FAILED,
    RA_MEDIA_INVALID_ARGUMENT,
} raplayer_errno_t;

typedef struct {
    uint64_t fd;
    struct sockaddr_in addr;
} ra_sock_local_t;

typedef struct {
    ra_sock_local_t *local_sock;
    struct sockaddr_in addr;
} ra_sock_remote_t;

const char *raplayer_strerror(int64_t err);

u_int16_t ra_swap_endian_uint16(u_int16_t number);

u_int32_t ra_swap_endian_uint32(u_int32_t number);

bool ra_compare_sockaddr(struct sockaddr_in *s1, struct sockaddr_in *s2);

#endif //RAPLAYER_UTILS_H
