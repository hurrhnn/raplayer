#include <stdbool.h>
#include <stdint.h>
#include <opus/opus.h>

#ifndef RAPLAYER_MEDIA_H
#define RAPLAYER_MEDIA_H

#define RA_MEDIA_TYPE_LOCAL_PROVIDE 0
#define RA_MEDIA_TYPE_LOCAL_CONSUME 1
#define RA_MEDIA_TYPE_REMOTE_PROVIDE 2

#include "utils.h"
#include "queue.h"

typedef enum {
    RA_MEDIA_INITIATED = (1 << 0),
    RA_MEDIA_TERMINATED = (1 << 1),
} ra_media_status_t;

typedef struct {
    uint64_t id;
    void *src;
    uint8_t type;
    ra_media_status_t status;
    union {
        void *(*send)(void *user_data);
        void (*recv)(void *frame, int frame_size, void *user_data);
    } callback;

    union {
        OpusEncoder *encoder;
        OpusDecoder *decoder;
    } opus;

    void *cb_user_data;

    struct {
        ra_queue_t *queue;
    } current;
} ra_media_t;

typedef struct {
    ra_media_t ***p_media;
    uint64_t *cnt_media;
    pthread_mutex_t *mutex;
    uint8_t type;
    uint64_t queue_size;
} ra_media_register_t;

int64_t ra_media_register(ra_media_register_t media_register_ctx, void *callback, void *cb_user_data);

#endif //RAPLAYER_MEDIA_H
