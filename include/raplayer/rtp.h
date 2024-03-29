#include <stdint.h>

#ifndef RAPLAYER_RTP_H
#define RAPLAYER_RTP_H

#include "utils.h"

typedef union {
    struct __attribute__((aligned(1), packed)) {
        u_int8_t csrc_count: 4;
        u_int8_t extension: 1;
        u_int8_t padding: 1;
        u_int8_t version: 2;

        u_int8_t payload_type: 7;
        u_int8_t marker: 1;

        u_int16_t sequence;
        u_int32_t timestamp;
        u_int32_t ssrc;
        u_int32_t *csrc;
    };
    uint8_t without_csrc_data[0xc];
    uint8_t combined_data[0x14];
} ra_rtp_t;

ra_rtp_t ra_rtp_get_context(void* buffer);

uint64_t ra_rtp_get_length(ra_rtp_t rtp_header);

void ra_rtp_init_context(ra_rtp_t* rtp_header);

void ra_rtp_set_next(ra_rtp_t *rtp_header, uint32_t timestamp);

#endif //RAPLAYER_RTP_H
