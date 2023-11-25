#ifndef RAPLAYER_UTILS_H
#define RAPLAYER_UTILS_H

#include <raplayer/config.h>
#include <stdlib.h>
#include <stdint.h>

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

uint64_t provide_20ms_opus_offset_calculator(unsigned char c_bits[MAX_DATA_SIZE], unsigned char **result);

#endif //RAPLAYER_UTILS_H
