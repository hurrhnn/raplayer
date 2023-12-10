#include <raplayer/dispatcher.h>
#include <raplayer/node.h>
#include <raplayer/queue.h>

void *dispatch_packet(void *p_ra_packet_dispatcher_args) {
    ra_node_t *node = p_ra_packet_dispatcher_args;
    while (!(node->status & RA_NODE_CONNECTION_EXHAUSTED)) {
        ra_task_t *task = retrieve_task(node->recv_queue);
        if(memcmp(task->data, RA_CTL_HEADER, DWORD) == 0) {
            uint8_t type = -1;
            memcpy(&type, task->data + DWORD, BYTE);
            switch (type) {
                case 0x00:
                    if(!(node->status & RA_NODE_CONNECTED)) {
                        void* heartbeat_msg = malloc(DWORD + BYTE);
                        memcpy(heartbeat_msg, RA_CTL_HEADER, DWORD);
                        memcpy(heartbeat_msg + DWORD, "\x00", BYTE);

                        sendto((int) node->local_sock->fd, heartbeat_msg, DWORD + BYTE, 0,
                               (struct sockaddr *) &node->remote_sock->addr,
                               sizeof(struct sockaddr_in));
                        node->status |= RA_NODE_CONNECTED;
                    }
                    node->status |= RA_NODE_HEARTBEAT_RECEIVED;
                    break;
                case 0x01:
                    memcpy(&node->channels, (task->data + DWORD), WORD);
                    memcpy(&node->sample_rate, (task->data + DWORD + WORD), DWORD);
                    memcpy(&node->bit_per_sample, (task->data + DWORD + WORD + DWORD), WORD);
                    break;
                case 0x02:
                    memcpy(&node->crypto_context, (task->data + DWORD), sizeof(struct chacha20_context));
                default:
                    break;
            }
            remove_task(task);
        } else
            append_task(node->frame_queue, task);
    }
    return NULL;
}