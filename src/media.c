#include <stdio.h>

#include "raplayer.h"
#include "raplayer/media.h"

int64_t raplayer_register_media_provider(raplayer_t *raplayer, void *(*send_callback)(void *user_data),
                                         void *send_cb_user_data) {
    if (send_callback != NULL) {
        void *before_address = raplayer->media;
        ra_realloc(raplayer->media, sizeof(ra_media_t * ) * ((raplayer->cnt_media) + 1));
        RA_DEBUG_MORE(GRN, "Reallocated media context %p to %p, size: 0x%llX\n",
                      before_address,
                      raplayer->media,
                      sizeof(ra_media_t * ) *
                      ((raplayer->cnt_media) + 1));

        raplayer->media[raplayer->cnt_media] = malloc(sizeof(ra_media_t));
        raplayer->media[raplayer->cnt_media]->type = RA_MEDIA_TYPE_SEND;
        raplayer->media[raplayer->cnt_media]->callback.send = send_callback;
        raplayer->media[raplayer->cnt_media]->cb_user_data = send_cb_user_data;
        raplayer->media[raplayer->cnt_media]->src_sock = raplayer->local_sock[raplayer->cnt_local_sock - 1];
        raplayer->media[raplayer->cnt_media]->current.sequence = 0;
        pthread_rwlock_init(&raplayer->media[raplayer->cnt_media]->current.rwlock, NULL);

        RA_DEBUG_MORE(GRN, "Media provider %p successfully registered to context.\n", send_callback);
        return (int64_t) raplayer->cnt_media++;
    } else
        return RA_MEDIA_INVALID_ARGUMENT;
}

int64_t raplayer_register_media_consumer(raplayer_t *raplayer,
                                         void (*recv_callback)(void *frame, int frame_size, void *user_data),
                                         void *recv_cb_user_data) {
    if (recv_callback != NULL) {
        void *before_address = raplayer->media;
        ra_realloc(raplayer->media, sizeof(ra_media_t * ) * ((raplayer->cnt_media) + 1));
        RA_DEBUG_MORE(GRN, "Reallocated media context %p to %p, size: 0x%llX\n",
                      before_address,
                      raplayer->media,
                      sizeof(ra_media_t * ) *
                      ((raplayer->cnt_media) + 1));

        raplayer->media[raplayer->cnt_media] = malloc(sizeof(ra_media_t));
        raplayer->media[raplayer->cnt_media]->type = RA_MEDIA_TYPE_RECV;
        raplayer->media[raplayer->cnt_media]->callback.recv = recv_callback;
        raplayer->media[raplayer->cnt_media]->cb_user_data = recv_cb_user_data;
        raplayer->media[raplayer->cnt_media]->src_sock = raplayer->local_sock[raplayer->cnt_local_sock - 1];
        raplayer->media[raplayer->cnt_media]->current.sequence = 0;
        pthread_rwlock_init(&raplayer->media[raplayer->cnt_media]->current.rwlock, NULL);

        RA_DEBUG_MORE(GRN, "Media consumer %p successfully registered to context.\n", recv_callback);
        return (int64_t) raplayer->cnt_media++;
    } else
        return RA_MEDIA_INVALID_ARGUMENT;
}