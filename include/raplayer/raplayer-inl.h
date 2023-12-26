#ifndef RAPLAYER_RAPLAYER_INL_H
#define RAPLAYER_RAPLAYER_INL_H

#include "node.h"
#include "media.h"

typedef struct {
    uint64_t cnt_fds;
    struct pollfd *fds;

    uint64_t cnt_local_sock;
    ra_sock_local_t **local_sock;

    uint64_t cnt_remote_sock;
    ra_sock_remote_t **remote_sock;

    uint64_t cnt_node;
    ra_node_t **node;

    uint64_t cnt_media;
    ra_media_t **media;
    pthread_mutex_t media_mutex;
} raplayer_t;

#endif //RAPLAYER_RAPLAYER_INL_H
