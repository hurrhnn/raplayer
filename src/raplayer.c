#include <netdb.h>
#include <raplayer.h>

void raplayer_init_context(raplayer_t *raplayer) {
    memset(raplayer, 0x0, sizeof (raplayer_t));
    ra_packet_scheduler_args_t *scheduler_args = malloc(sizeof (ra_packet_scheduler_args_t));
    scheduler_args->cnt_fds = &raplayer->cnt_fds;
    scheduler_args->fds = &raplayer->fds;

    scheduler_args->cnt_local_sock = &raplayer->cnt_local_sock;
    scheduler_args->local_sock = &raplayer->local_sock;

    scheduler_args->cnt_remote_sock = &raplayer->cnt_remote_sock;
    scheduler_args->remote_sock = &raplayer->remote_sock;

    scheduler_args->cnt_node = &raplayer->cnt_node;
    scheduler_args->node = &raplayer->node;

    scheduler_args->spawn = &raplayer->spawn;
    scheduler_args->cnt_spawn = &raplayer->cnt_spawn;

    pthread_t packet_scheduler;
    pthread_create(&packet_scheduler, NULL, schedule_packet, scheduler_args);
}

int32_t raplayer_spawn(raplayer_t *raplayer, bool mode, char *address, int port, void *(*send_callback) (void *user_data),
                       void *send_cb_user_data, void (*recv_callback) (void *frame, int frame_size, void *user_data), void *recv_cb_user_data) {

    void *before_address = raplayer->fds;
    ra_realloc(raplayer->fds, sizeof(struct pollfd) * ((raplayer->cnt_fds) + 1));
    RA_DEBUG_MORE(GRN, "Reallocated poll fds context %p to %p, size: 0x%llX\n",
                  before_address,
                  raplayer->fds,
                  sizeof(struct pollfd) *
                  ((raplayer->cnt_fds) + 1));

    before_address = raplayer->local_sock;
    ra_realloc(raplayer->local_sock, sizeof(ra_sock_local_t *) * ((raplayer->cnt_local_sock) + 1));
    RA_DEBUG_MORE(GRN, "Reallocated local sock context %p to %p, size: 0x%llX\n",
                  before_address,
                  raplayer->local_sock,
                  sizeof(ra_sock_local_t *) *
                  ((raplayer->cnt_local_sock) + 1));

    int sock_fd;
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        return RA_SOCKET_CREATION_FAILED;

    raplayer->local_sock[raplayer->cnt_local_sock] = malloc(sizeof(ra_sock_local_t));
    raplayer->local_sock[raplayer->cnt_local_sock]->fd = sock_fd;
    raplayer->local_sock[raplayer->cnt_local_sock]->addr.sin_family = AF_INET;
    raplayer->local_sock[raplayer->cnt_local_sock]->addr.sin_port = htons((uint16_t) port);
    if(address == NULL)
        raplayer->local_sock[raplayer->cnt_local_sock]->addr.sin_addr.s_addr = htonl(INADDR_ANY);
    else {
        struct hostent *hostent;
        struct in_addr **addr_list;

        if ((hostent = gethostbyname(address)) == NULL) {
            return RA_CONNECTION_RESOLVE_FAILED;
        } else {
            addr_list = (struct in_addr **) hostent->h_addr_list;
            char *resolved_address = malloc(0x100);
            strcpy(resolved_address, inet_ntoa(*addr_list[0]));

            if (!inet_pton(AF_INET, resolved_address, &raplayer->local_sock[raplayer->cnt_local_sock]->addr.sin_addr))
                return RA_ADDRESS_CONVERSION_FAILED;
            free(resolved_address);
        }
    }

    if(mode == RA_MODE_HOST) {
        /* Bind the socket with local address. */
        if((bind(sock_fd, (struct sockaddr *) &raplayer->local_sock[raplayer->cnt_local_sock]->addr,
                 sizeof(struct sockaddr_in))) < 0) {
            return RA_SOCKET_BIND_FAILED;
        }
    }

    raplayer->fds[raplayer->cnt_fds].fd = sock_fd;
    raplayer->fds[raplayer->cnt_fds].events = POLLIN;

    raplayer->cnt_local_sock++;
    raplayer->cnt_fds++;

    if(mode == RA_MODE_PEER) {
        void* heartbeat_msg = malloc(0x05);
        memcpy(heartbeat_msg, RA_CTL_HEADER, DWORD);
        memcpy(heartbeat_msg + DWORD, "\x00", BYTE);
        sendto(sock_fd, heartbeat_msg, 0x05, 0,
               (const struct sockaddr *) &raplayer->local_sock[raplayer->cnt_local_sock - 1]->addr,
                       sizeof(struct sockaddr_in));
        free(heartbeat_msg);
    }

    if(send_callback != NULL) {
        before_address = raplayer->spawn;
        ra_realloc(raplayer->spawn, sizeof(ra_spawn_t *) * ((raplayer->cnt_spawn) + 1));
        RA_DEBUG_MORE(GRN, "Reallocated send spawn context %p to %p, size: 0x%llX\n",
                      before_address,
                      raplayer->spawn,
                      sizeof(ra_spawn_t *) *
                      ((raplayer->cnt_spawn) + 1));

        raplayer->spawn[raplayer->cnt_spawn] = malloc(sizeof(ra_spawn_t));
        raplayer->spawn[raplayer->cnt_spawn]->type = RA_SPAWN_TYPE_SEND;
        raplayer->spawn[raplayer->cnt_spawn]->callback.send = send_callback;
        raplayer->spawn[raplayer->cnt_spawn]->cb_user_data = send_cb_user_data;
        raplayer->spawn[raplayer->cnt_spawn]->local_sock = raplayer->local_sock[raplayer->cnt_local_sock - 1];
        raplayer->cnt_spawn++;
    }

    if(recv_callback != NULL) {
        before_address = raplayer->spawn;
        ra_realloc(raplayer->spawn, sizeof(ra_spawn_t *) * ((raplayer->cnt_spawn) + 1));
        RA_DEBUG_MORE(GRN, "Reallocated recv spawn context %p to %p, size: 0x%llX\n",
                      before_address,
                      raplayer->spawn,
                      sizeof(ra_spawn_t *) *
                      ((raplayer->cnt_spawn) + 1));

        raplayer->spawn[raplayer->cnt_spawn] = malloc(sizeof(ra_spawn_t));
        raplayer->spawn[raplayer->cnt_spawn]->type = RA_SPAWN_TYPE_RECV;
        raplayer->spawn[raplayer->cnt_spawn]->callback.recv = recv_callback;
        raplayer->spawn[raplayer->cnt_spawn]->cb_user_data = recv_cb_user_data;
        raplayer->spawn[raplayer->cnt_spawn]->local_sock = raplayer->local_sock[raplayer->cnt_local_sock - 1];
        raplayer->cnt_spawn++;
    }
    return 0;
}