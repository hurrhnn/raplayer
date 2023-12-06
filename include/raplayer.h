#ifndef RAPLAYER_RAPLAYER_H
#define RAPLAYER_RAPLAYER_H

#include <raplayer/utils.h>
#include <raplayer/client.h>
#include <raplayer/server.h>
#include <raplayer/chacha20.h>
#include <raplayer/task_queue.h>
#include <raplayer/task_scheduler.h>

typedef struct {
    ra_client_t *client;
    ra_server_t *server;
} raplayer_t;

void raplayer_init_context(raplayer_t *raplayer);

uint64_t raplayer_spawn_server(raplayer_t *raplayer, int port, uint8_t *(*frame_callback)(void *user_data),
                               void *callback_user_data);

uint64_t raplayer_spawn_client(raplayer_t *raplayer, char *address, int port,
                               void(*frame_callback)(void *frame, int frame_size, void *user_data),
                               void *callback_user_data);

int8_t raplayer_wait_server(raplayer_t *raplayer, uint64_t idx);

int8_t raplayer_wait_client(raplayer_t *raplayer, uint64_t idx);

int raplayer_get_server_status(raplayer_t *raplayer, uint64_t idx);

int raplayer_get_client_status(raplayer_t *raplayer, uint64_t idx);

void raplayer_set_client_status(raplayer_t *raplayer, uint64_t idx, int status);

void raplayer_set_server_status(raplayer_t *raplayer, uint64_t idx, int status);

#endif //RAPLAYER_RAPLAYER_H
