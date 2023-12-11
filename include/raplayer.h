#ifndef RAPLAYER_RAPLAYER_H
#define RAPLAYER_RAPLAYER_H

#include "raplayer/utils.h"
#include "raplayer/node.h"

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
} raplayer_t;

void raplayer_init_context(raplayer_t *raplayer);

int64_t raplayer_spawn(raplayer_t *raplayer, bool mode, char *address, int port);

int64_t raplayer_register_media_provider(raplayer_t *raplayer, void *(*send_callback)(void *user_data),
                                         void *send_cb_user_data);


int64_t raplayer_register_media_consumer(raplayer_t *raplayer,
                                         void (*recv_callback)(void *frame, int frame_size, void *user_data),
                                         void *recv_cb_user_data);

#endif //RAPLAYER_RAPLAYER_H
