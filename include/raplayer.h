#include "raplayer/raplayer-inl.h"

#ifndef RAPLAYER_H
#define RAPLAYER_H

void raplayer_init_context(raplayer_t *raplayer);

int64_t raplayer_spawn(raplayer_t *raplayer, bool mode, char *address, int port);

int64_t raplayer_register_media_provider(raplayer_t *raplayer, uint64_t spawn_id, void *(*send_callback)(void *user_data),
                                         void *send_cb_user_data);

int64_t raplayer_register_media_consumer(raplayer_t *raplayer, uint64_t spawn_id,
                                         void (*recv_callback)(void *frame, int frame_size, void *user_data),
                                         void *recv_cb_user_data);

void raplayer_wait(raplayer_t *raplayer, uint64_t spawn_id);

#endif //RAPLAYER_H
