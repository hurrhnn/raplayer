#include "raplayer/dispatcher.h"
#include "raplayer/node.h"

void *dispatch_packet(void *p_ra_packet_dispatcher_args) {
    ra_node_t *node = p_ra_packet_dispatcher_args;
    while (!(node->status & RA_NODE_CONNECTION_EXHAUSTED)) {
        ra_task_t *task = dequeue_task(node->recv_queue);
        if(memcmp(task->data, RA_CTL_HEADER, RA_DWORD) == 0) {
            uint8_t type = -1;
            memcpy(&type, task->data + RA_DWORD, RA_BYTE);
            switch (type) {
                case 0x00:
                    if(!(node->status & RA_NODE_CONNECTED)) {
                        void* heartbeat_msg = malloc(RA_DWORD + RA_BYTE);
                        memcpy(heartbeat_msg, RA_CTL_HEADER, RA_DWORD);
                        memcpy(heartbeat_msg + RA_DWORD, "\x00", RA_BYTE);

                        sendto((int) node->local_sock->fd, heartbeat_msg, RA_DWORD + RA_BYTE, 0,
                               (struct sockaddr *) &node->remote_sock->addr,
                               sizeof(struct sockaddr_in));
                        node->status |= RA_NODE_CONNECTED;
                    }
                    node->status |= RA_NODE_HEARTBEAT_RECEIVED;
                    break;
                case 0x01:
                    memcpy(&node->channels, (task->data + RA_DWORD), RA_WORD);
                    memcpy(&node->sample_rate, (task->data + RA_DWORD + RA_WORD), RA_DWORD);
                    memcpy(&node->bit_per_sample, (task->data + RA_DWORD + RA_WORD + RA_DWORD), RA_WORD);
                    break;
                case 0x02:
                    memcpy(&node->crypto_context, (task->data + RA_DWORD), sizeof(struct chacha20_context));
                default:
                    break;
            }
            destroy_task(task);
        } else {
            enqueue_task_with_removal(node->remote_media->current.queue, task);
            node->remote_media->current.sequence++;
        }

    }
    return NULL;
}