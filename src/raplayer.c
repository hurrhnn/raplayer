#include <raplayer.h>

void raplayer_init_context(raplayer_t *raplayer) {
    raplayer->client = malloc(sizeof(ra_client_t));
    raplayer->client->list = malloc(sizeof(*raplayer->client->list));
    raplayer->client->idx = -1;

    raplayer->server = malloc(sizeof(ra_server_t));
    raplayer->server->list = malloc(sizeof(*raplayer->server->list));
    raplayer->server->idx = -1;
}

uint64_t raplayer_spawn_server(raplayer_t *raplayer, int port, uint8_t *(*frame_callback)(void *user_data),
                               void *callback_user_data) {
    uint64_t idx = ++raplayer->server->idx;
    raplayer->server->list = realloc(raplayer->server->list, sizeof(*raplayer->server->list) * (idx + 2));
    memset(&(raplayer->server->list[idx]), 0x0, sizeof(*raplayer->server->list));

    raplayer->server->list[idx].port = port;
    raplayer->server->list[idx].frame_callback = frame_callback;
    raplayer->server->list[idx].callback_user_data = callback_user_data;
    raplayer->server->list[idx].status = 0;

    pthread_create(&raplayer->server->list[idx].thread, NULL, ra_server, raplayer->server);
    return idx;
}

uint64_t raplayer_spawn_client(raplayer_t *raplayer, char *address, int port,
                               void(*frame_callback)(void *frame, int frame_size, void *user_data),
                               void *callback_user_data) {
    uint64_t idx = ++raplayer->client->idx;
    raplayer->client->list = realloc(raplayer->client->list, sizeof(*raplayer->client->list) * (idx + 2));
    memset(&(raplayer->client->list[idx]), 0x0, sizeof(*raplayer->client->list));

    raplayer->client->list[idx].address = address;
    raplayer->client->list[idx].port = port;
    raplayer->client->list[idx].frame_callback = frame_callback;
    raplayer->client->list[idx].callback_user_data = callback_user_data;
    raplayer->client->list[idx].status = 0;

    pthread_create(&raplayer->client->list[idx].thread, NULL, ra_client, raplayer->client);
    return idx;
}

int32_t raplayer_wait_server(raplayer_t *raplayer, uint64_t idx) {
    int32_t server_status;
    pthread_join(raplayer->server->list[idx].thread, (void **) &server_status);

    return server_status;
}

int32_t raplayer_wait_client(raplayer_t *raplayer, uint64_t idx) {
    int32_t client_status;
    pthread_join(raplayer->client->list[idx].thread, (void **) &client_status);

    return client_status;
}

bool raplayer_get_server_status(raplayer_t *raplayer, uint64_t idx, ra_node_status_t type) {
    return (raplayer->server->list[idx].status & type);
}

void raplayer_set_server_status(raplayer_t *raplayer, uint64_t idx, ra_node_status_t type) {
    switch (type) {
        case RA_NODE_INITIATED:
            raplayer->server->list[idx].status = RA_NODE_INITIATED;
            break;
        case RA_NODE_CONNECTED:
            raplayer->server->list[idx].status = (RA_NODE_INITIATED | RA_NODE_CONNECTED);
            break;
        case RA_NODE_CONNECTION_EXHAUSTED:
            raplayer->server->list[idx].status = RA_NODE_CONNECTION_EXHAUSTED;
            break;
        case RA_NODE_HEARTBEAT_RECEIVED:
            raplayer->server->list[idx].status |= RA_NODE_HEARTBEAT_RECEIVED;
            break;
    }
}

bool raplayer_get_client_status(raplayer_t *raplayer, uint64_t idx, ra_node_status_t type) {
    return (raplayer->client->list[idx].status & type);
}

void raplayer_set_client_status(raplayer_t *raplayer, uint64_t idx, ra_node_status_t type) {
    switch (type) {
        case RA_NODE_INITIATED:
            raplayer->client->list[idx].status = RA_NODE_INITIATED;
            break;
        case RA_NODE_CONNECTED:
            raplayer->client->list[idx].status = (RA_NODE_INITIATED | RA_NODE_CONNECTED);
            break;
        case RA_NODE_CONNECTION_EXHAUSTED:
            raplayer->client->list[idx].status = RA_NODE_CONNECTION_EXHAUSTED;
            break;
        case RA_NODE_HEARTBEAT_RECEIVED:
            raplayer->client->list[idx].status |= RA_NODE_HEARTBEAT_RECEIVED;
            break;
    }
}