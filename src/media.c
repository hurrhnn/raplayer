#include <stdio.h>
#include <unistd.h>
#include "raplayer/raplayer-inl.h"
#include "raplayer/node.h"

int64_t ra_media_register(ra_media_register_t media_register_ctx, void *callback, void *cb_user_data) {
    pthread_mutex_lock(media_register_ctx.mutex);
    int err;
    ra_media_t **media = *media_register_ctx.p_media;
    void *before_address = media;
    ra_realloc(media, sizeof(ra_media_t *) * (*media_register_ctx.cnt_media + 1));
    RA_DEBUG_MORE(GRN, "Reallocated media context %p to %p, size: 0x%llX\n",
                  before_address,
                  media,
                  sizeof(ra_media_t *) *
                  ((*media_register_ctx.cnt_media) + 1));
    *media_register_ctx.p_media = media;

    media[*media_register_ctx.cnt_media] = malloc(sizeof(ra_media_t));
    uint64_t id = media[*media_register_ctx.cnt_media]->id = *media_register_ctx.cnt_media;

    media[id]->src = NULL;
    media[id]->type = media_register_ctx.type;
    if (media_register_ctx.type == RA_MEDIA_TYPE_LOCAL_PROVIDE ||
        media_register_ctx.type == RA_MEDIA_TYPE_REMOTE_PROVIDE) {
        media[id]->callback.send = (void *(*)(void *)) callback;

        /* Create a new opus encoder state. */
        media[id]->opus.encoder = opus_encoder_create((opus_int32) RA_OPUS_AUDIO_SR, RA_OPUS_AUDIO_CH,
                                                      RA_OPUS_APPLICATION, &err);
        if (err < 0) {
            RA_ERROR("Error: failed to create an encoder - %s\n", opus_strerror(err));
            pthread_mutex_unlock(media_register_ctx.mutex);
            return RA_CREATE_OPUS_ENCODER_FAILED;
        }

        if (opus_encoder_ctl(media[id]->opus.encoder,
                             OPUS_SET_BITRATE(RA_OPUS_AUDIO_SR * RA_OPUS_AUDIO_CH)) <
            0) {
            RA_ERROR("Error: failed to set bitrate - %s\n", opus_strerror(err));
            pthread_mutex_unlock(media_register_ctx.mutex);
            return RA_OPUS_ENCODER_CTL_FAILED;
        }
    } else {
        media[id]->callback.recv = (void (*)(void *, int, void *)) callback;

        /* Create a new opus decoder state. */
        media[id]->opus.decoder = opus_decoder_create(RA_OPUS_AUDIO_SR, RA_OPUS_AUDIO_CH, &err);
        if (err < 0) {
            RA_ERROR("Error: failed to create an decoder - %s\n", opus_strerror(err));
            pthread_mutex_unlock(media_register_ctx.mutex);
            return RA_CREATE_OPUS_DECODER_FAILED;
        }
    }

    media[id]->cb_user_data = cb_user_data;
    media[id]->current.queue = malloc(sizeof(ra_queue_t));
    init_queue(media[id]->current.queue, media_register_ctx.queue_size);

    RA_DEBUG_MORE(GRN, "Media %s id %llu with%s callback successfully registered to context.\n",
                  (media_register_ctx.type == RA_MEDIA_TYPE_LOCAL_PROVIDE ||
                   media_register_ctx.type == RA_MEDIA_TYPE_REMOTE_PROVIDE) ? "provider"
                                                                            : "consumer", id,
                  callback == NULL ? "out" : "");
    (*media_register_ctx.cnt_media) = (*media_register_ctx.cnt_media) + 1;
    pthread_mutex_unlock(media_register_ctx.mutex);
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
        if (media->type == RA_MEDIA_TYPE_LOCAL_PROVIDE) {
            ra_rtp_t rtp_header;
            ra_rtp_init_context(&rtp_header);
            opus_int16 in[RA_FRAME_SIZE * RA_OPUS_AUDIO_CH];
            unsigned char c_bits[RA_FRAME_SIZE];

            struct timespec start_timespec, current_time, calculated_delay;
            clock_gettime(CLOCK_MONOTONIC, &start_timespec);
            time_t start_time = (start_timespec.tv_sec * 1000000000L) + start_timespec.tv_nsec, time, offset = 0L;
            while (!(media->status & RA_MEDIA_TERMINATED)) {
                offset += 20000000L;
                time = start_time + offset;

                /* Initiate the payload. */
                ra_task_t *frame = create_task(RA_MAX_DATA_SIZE);
                memset(frame->data, 0x0, frame->data_len);

                /* Set rtp context. */
                ra_rtp_set_next(&rtp_header, 960);
                uint16_t rtp_header_len = ra_rtp_get_length(rtp_header);

                /* Retrieve pcm frame from callback. */
                unsigned char *pcm_bytes = frame->data;
                memcpy(frame->data, media->callback.send(media->cb_user_data),
                       RA_FRAME_SIZE * RA_OPUS_AUDIO_CH * RA_WORD);

                /* Convert from little-endian ordering. */
                for (int i = 0; i < RA_FRAME_SIZE * RA_OPUS_AUDIO_CH; i++)
                    in[i] = (opus_int16) (pcm_bytes[2 * i + 1] << 8 | pcm_bytes[2 * i]);

                /* Encode the frame. */
                int nbBytes = opus_encode(media->opus.encoder, in, RA_FRAME_SIZE, c_bits, RA_FRAME_SIZE);
                if (nbBytes < 0) {
                    RA_ERROR("Error: opus encode failed - %s\n", opus_strerror(nbBytes));
                    media->status = RA_MEDIA_TERMINATED;
                    return (void *) RA_OPUS_ENCODE_FAILED;
                }

                /* Construct the payload. */
                frame->data_len = (ssize_t) (rtp_header_len + nbBytes);
                memcpy(frame->data, rtp_header.without_csrc_data, rtp_header_len);
                memcpy(frame->data + rtp_header_len, c_bits, nbBytes);

                clock_gettime(CLOCK_MONOTONIC, &current_time);
                time -= ((current_time.tv_sec * 1000000000L) + current_time.tv_nsec);

                calculated_delay.tv_sec = ((time / 1000000000L) > 0 ? (time / 1000000000L) : 0);
                calculated_delay.tv_nsec = ((time % 1000000000L) > 0 ? (time % 1000000000L) : 0);
                nanosleep(&calculated_delay, NULL);

                enqueue_task_with_removal(media->current.queue, frame);
            }
        } else if (media->type == RA_MEDIA_TYPE_LOCAL_CONSUME) {
            opus_int16 *out = malloc(RA_MAX_FRAME_SIZE * RA_WORD * RA_OPUS_AUDIO_CH);
            unsigned char *pcm_bytes = malloc(RA_MAX_FRAME_SIZE * RA_WORD * RA_OPUS_AUDIO_CH);

            while (!(media->status & RA_MEDIA_TERMINATED)) {
                ra_task_t *frame = dequeue_task(media->current.queue);

                /* Decrypt the frame. */
//              chacha20_xor(&ctx, *calculated_c_bits, nbBytes);

                /* Decode the frame. */
                int frame_size = opus_decode(media->opus.decoder, (unsigned char *) frame->data,
                                             (opus_int32) (frame->data_len), out, RA_MAX_FRAME_SIZE, 0);

                if (frame_size < 0) {
                    printf("Error: Opus decoder failed - %s\n", opus_strerror(frame_size));
                    media->status = RA_MEDIA_TERMINATED;
                    return (void *) RA_OPUS_DECODE_FAILED;
                } else
                    frame_size *= (RA_OPUS_AUDIO_CH * RA_WORD);

                /* Convert to little-endian ordering. */
                for (int i = 0; i < frame_size / RA_WORD; i++) {
                    pcm_bytes[2 * i] = out[i] & 0xFF;
                    pcm_bytes[2 * i + 1] = (out[i] >> 8) & 0xFF;
                }
                media->callback.recv(pcm_bytes, (int) frame_size, media->cb_user_data);
                destroy_task(frame);
            }
        }
    }

    media->status = RA_MEDIA_TERMINATED;
    return EXIT_SUCCESS;
}

int64_t
raplayer_register_media_provider(raplayer_t *raplayer, uint64_t spawn_id, void *(*send_callback)(void *user_data),
                                 void *send_cb_user_data) {
    if (send_callback != NULL) {
        ra_media_register_t media_register = {&raplayer->media, &raplayer->cnt_media, &raplayer->media_mutex,
                                              RA_MEDIA_TYPE_LOCAL_PROVIDE, 7};
        int64_t media_id = ra_media_register(media_register, send_callback, send_cb_user_data);
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
        ra_media_register_t media_register = {&raplayer->media, &raplayer->cnt_media, &raplayer->media_mutex,
                                              RA_MEDIA_TYPE_LOCAL_CONSUME, 7};
        int64_t media_id = ra_media_register(media_register, recv_callback, recv_cb_user_data);
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