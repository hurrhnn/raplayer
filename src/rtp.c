#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "raplayer/rtp.h"
#include "raplayer/chacha20.h"

ra_rtp_t ra_rtp_get_context(void* buffer) {
    ra_rtp_t rtp_header;
    memcpy(&rtp_header, buffer, sizeof(rtp_header.without_csrc_data));

    if(rtp_header.csrc_count != 0)
    {
        rtp_header.csrc = malloc(sizeof(uint32_t) * rtp_header.csrc_count);
        uint32_t *csrc = rtp_header.csrc;
        for(int i = 0; i < rtp_header.csrc_count; i++)
            memcpy(&csrc[i], buffer + (sizeof(rtp_header.without_csrc_data) + (i * sizeof (uint32_t))), RA_DWORD);
    } else
        rtp_header.csrc = NULL;
    return rtp_header;
}

uint64_t ra_rtp_get_length(ra_rtp_t rtp_header) {
    return (sizeof(rtp_header.without_csrc_data) + (rtp_header.csrc_count * sizeof(uint32_t)));
}

void ra_rtp_init_context(ra_rtp_t* rtp_header) {
    uint32_t *ssrc = (uint32_t *) generate_random_bytestream(sizeof(uint32_t));

    rtp_header->version = 2;
    rtp_header->padding = 0;
    rtp_header->extension = 0;
    rtp_header->csrc_count = 0;
    rtp_header->marker = 0;
    rtp_header->payload_type = 120;
    rtp_header->sequence = 0;
    rtp_header->timestamp = 0;
    rtp_header->ssrc = ra_swap_endian_uint32(*ssrc);
    rtp_header->csrc = NULL;
}

void ra_rtp_set_next(ra_rtp_t *rtp_header, uint32_t timestamp) {
    rtp_header->sequence = ra_swap_endian_uint16((ra_swap_endian_uint16(rtp_header->sequence) + 1) % 0x10000);
    rtp_header->timestamp = ra_swap_endian_uint32((ra_swap_endian_uint32(rtp_header->timestamp) + timestamp) % 0x100000000);
}