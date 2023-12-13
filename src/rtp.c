#include <string.h>
#include <stdlib.h>
#include "raplayer/rtp.h"

ra_rtp_t ra_get_rtp_context(void* buffer) {
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

uint64_t ra_get_rtp_length(ra_rtp_t rtp_header) {
    return (sizeof(rtp_header.without_csrc_data) + (rtp_header.csrc_count * sizeof(uint32_t)));
}
