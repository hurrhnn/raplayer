#ifndef RAPLAYER_NODE_H
#define RAPLAYER_NODE_H

#include <stdint.h>
#include <opus/opus.h>
#include <pthread.h>
#include <raplayer/task_queue.h>

#define RA_SPAWN_TYPE_SEND 0
#define RA_SPAWN_TYPE_RECV 1

typedef struct {
    bool type;
    union {
        void *(*send)(void *user_data);
        void (*recv)(void *frame, int frame_size, void *user_data);
    } callback;
    void *cb_user_data;

    ra_sock_local_t *local_sock;
} ra_spawn_t;

typedef struct {
    ra_node_status_t status;
    uint64_t id;

    struct {
        uint16_t channels;
        int32_t sample_rate;
        uint16_t bit_per_sample;
    };

    ra_sock_local_t *local_sock;
    ra_sock_remote_t *remote_sock;

    struct chacha20_context crypto_context;

    ra_task_queue_t *recv_queue;
    ra_task_queue_t *send_queue;
    ra_task_queue_t *frame_queue;
} ra_node_t;

typedef struct {
    ra_node_t *node;
    ra_spawn_t ***spawn;
    uint64_t *cnt_spawn;
} ra_node_frame_args_t;

typedef struct {
    ra_node_t *node;
    int *turn;
    ra_spawn_t ***spawn;
    uint64_t *cnt_spawn;

    pthread_mutex_t *opus_builder_mutex;
    pthread_cond_t *opus_builder_cond;
} ra_opus_builder_args_t;

typedef struct {
    ra_node_t *node;

    pthread_mutex_t *opus_sender_mutex;
    pthread_cond_t *opus_sender_cond;
} ra_opus_sender_args_t;

typedef struct {
    ra_node_t *node;
    int *turn;

    pthread_mutex_t *opus_builder_mutex;
    pthread_cond_t *opus_builder_cond;
    pthread_mutex_t *opus_sender_mutex;
    pthread_cond_t *opus_sender_cond;
} ra_opus_timer_args_t;

void *ra_check_heartbeat(void *p_heartbeat_checker_args);

void *ra_send_heartbeat(void *p_heartbeat_checker_args);

void *ra_node_frame_receiver(void *p_node_frame_args);

void *ra_node_frame_sender(void *p_node_frame_args);

#endif //RAPLAYER_NODE_H
