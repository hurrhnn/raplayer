#include <stdio.h>
#include "raplayer/raplayer-inl.h"
#include "raplayer/node.h"

int64_t ra_media_register(ra_media_t ***p_media, uint64_t *cnt_media, uint8_t type,
                          uint64_t queue_size, void *callback,
                          void *cb_user_data) {
    ra_media_t **media = *p_media;
    void *before_address = media;
    ra_realloc(media, sizeof(ra_media_t *) * (*cnt_media + 1));
    RA_DEBUG_MORE(GRN, "Reallocated media context %p to %p, size: 0x%llX\n",
                  before_address,
                  media,
                  sizeof(ra_media_t *) *
                  ((*cnt_media) + 1));
    *p_media = media;

    media[*cnt_media] = malloc(sizeof(ra_media_t));
    uint64_t id = media[*cnt_media]->id = *cnt_media;
    media[id]->src = NULL;
    media[id]->type = type;
    if (type == RA_MEDIA_TYPE_LOCAL_PROVIDE || type == RA_MEDIA_TYPE_REMOTE_PROVIDE)
        media[id]->callback.send = (void *(*)(void *)) callback;
    else
        media[id]->callback.recv = (void (*)(void *, int, void *)) callback;
    media[id]->cb_user_data = cb_user_data;
    media[id]->current.queue = malloc(sizeof(ra_queue_t));
    init_queue(media[id]->current.queue, queue_size);
    media[id]->current.sequence = 0;

    (*cnt_media) = (*cnt_media) + 1;
    RA_DEBUG_MORE(GRN, "Media %s %p with%s callback successfully registered to context.\n",
                  (type == RA_MEDIA_TYPE_LOCAL_PROVIDE || type == RA_MEDIA_TYPE_REMOTE_PROVIDE) ? "provider"
                                                                                                : "consumer", media[id],
                  callback == NULL ? "out" : "");
    return (int64_t) id;
}

void *ra_media_process(void *p_media) {
    ra_media_t *media = p_media;
    if (media->callback.recv == NULL || media->callback.send == NULL)
        RA_DEBUG_MORE(YEL, "Media id %llu %s callback is not defined.\n", media->id,
                      (media->type == RA_MEDIA_TYPE_LOCAL_PROVIDE ? "provider" : "consumer"));
    else if (media->src == NULL) {
        RA_DEBUG_MORE(YEL, "Media id %llu source address is not set.\n", media->id);
    } else {
        RA_DEBUG_MORE(GRN, "Media id %llu media processing started.\n", media->id);
        while (!(media->status & RA_MEDIA_TERMINATED)) {
            if (media->type == RA_MEDIA_TYPE_LOCAL_PROVIDE) {
                ra_task_t *frame = create_task(RA_FRAME_SIZE * RA_OPUS_AUDIO_CH * RA_WORD);
                memcpy(frame->data, media->callback.send(media->cb_user_data),
                       RA_FRAME_SIZE * RA_OPUS_AUDIO_CH * RA_WORD);
                append_task(media->current.queue, frame);
                media->current.sequence++;
            } else if (media->type == RA_MEDIA_TYPE_LOCAL_CONSUME){
                ra_task_t *frame = retrieve_task(media->current.queue);
                if (frame == NULL)
                    media->callback.recv(NULL, 0, media->cb_user_data);
                else {
                    media->callback.recv(frame->data, (int) frame->data_len, media->cb_user_data);
                    media->current.sequence++;
                    remove_task(frame);
                }
            }
        }
    }
    return EXIT_SUCCESS;
}

int64_t
raplayer_register_media_provider(raplayer_t *raplayer, uint64_t spawn_id, void *(*send_callback)(void *user_data),
                                 void *send_cb_user_data) {
    if (send_callback != NULL) {
        int64_t media_id = ra_media_register(&raplayer->media, &raplayer->cnt_media,
                                             RA_MEDIA_TYPE_LOCAL_PROVIDE,
                                             5, send_callback, send_cb_user_data);
        if (media_id >= 0) {
            raplayer->media[media_id]->src = raplayer->local_sock[spawn_id];
            pthread_t media_process;
            pthread_create(&media_process, NULL, ra_media_process, raplayer->media[media_id]);
            raplayer->media[media_id]->status = RA_MEDIA_INITIATED;
        }
        return media_id;
    } else
        return RA_MEDIA_INVALID_ARGUMENT;
}

int64_t raplayer_register_media_consumer(raplayer_t *raplayer, uint64_t spawn_id,
                                         void (*recv_callback)(void *frame, int frame_size, void *user_data),
                                         void *recv_cb_user_data) {
    if (recv_callback != NULL) {
        int64_t media_id = ra_media_register(&raplayer->media, &raplayer->cnt_media,
                                             RA_MEDIA_TYPE_LOCAL_CONSUME,
                                             5, recv_callback, recv_cb_user_data);
        if (media_id >= 0) {
            raplayer->media[media_id]->src = raplayer->local_sock[spawn_id];
            pthread_t media_process;
            pthread_create(&media_process, NULL, ra_media_process, raplayer->media[media_id]);
            raplayer->media[media_id]->status = RA_MEDIA_INITIATED;
        }
        return media_id;
    } else
        return RA_MEDIA_INVALID_ARGUMENT;
}