#include <netdb.h>
#include <sys/poll.h>
#include <arpa/inet.h>

#include "raplayer.h"
#include "raplayer/scheduler.h"

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

    scheduler_args->media = &raplayer->media;
    scheduler_args->cnt_media = &raplayer->cnt_media;

    pthread_t packet_scheduler;
    pthread_create(&packet_scheduler, NULL, schedule_packet, scheduler_args);
}

int64_t raplayer_spawn(raplayer_t *raplayer, bool mode, char *address, int port) {

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

    uint64_t local_sock_idx = raplayer->cnt_local_sock++;
    raplayer->cnt_fds++;

    if(mode == RA_MODE_PEER) {
        void* heartbeat_msg = malloc(0x05);
        memcpy(heartbeat_msg, RA_CTL_HEADER, RA_DWORD);
        memcpy(heartbeat_msg + RA_DWORD, "\x00", RA_BYTE);
        sendto(sock_fd, heartbeat_msg, 0x05, 0,
               (const struct sockaddr *) &raplayer->local_sock[local_sock_idx]->addr,
                       sizeof(struct sockaddr_in));
        free(heartbeat_msg);
    }
    return (int64_t) local_sock_idx;
}