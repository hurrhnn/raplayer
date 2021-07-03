/*
 raplayer is a cross-platform remote audio player, written from the scratch.
 This file is part of raplayer.

 Copyright (C) 2021 Rhnn Hur (hurrhnn)

    raplayer is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "chacha20.h"

unsigned char *generate_random_bytestream(size_t num_bytes)
{
    struct timeval timeval;
    gettimeofday(&timeval, NULL);
    srandom(timeval.tv_usec);

    unsigned char *stream = malloc(num_bytes);
    for (size_t i = 0; i < num_bytes; i++)
        stream[i] = (char) random();

    return stream;
}

static uint32_t rotl_32(uint32_t x, int n) {
    return (x << n) | (x >> (32 - n));
}

static uint32_t pack_4bytes(const uint8_t *a) {
    uint32_t res = 0;
    res |= (uint32_t) a[0] << 0 * 8;
    res |= (uint32_t) a[1] << 1 * 8;
    res |= (uint32_t) a[2] << 2 * 8;
    res |= (uint32_t) a[3] << 3 * 8;
    return res;
}

static void chacha20_init_block(struct chacha20_context *ctx, uint8_t key[], uint8_t nonce[]) {
    memcpy(ctx->key, key, sizeof(ctx->key));
    memcpy(ctx->nonce, nonce, sizeof(ctx->nonce));

    const uint8_t *magic_constant = (uint8_t *) "DEADBEEFCAFEBABE";
    ctx->state[0] = pack_4bytes(magic_constant + 0 * 4);
    ctx->state[1] = pack_4bytes(magic_constant + 1 * 4);
    ctx->state[2] = pack_4bytes(magic_constant + 2 * 4);
    ctx->state[3] = pack_4bytes(magic_constant + 3 * 4);
    ctx->state[4] = pack_4bytes(key + 0 * 4);
    ctx->state[5] = pack_4bytes(key + 1 * 4);
    ctx->state[6] = pack_4bytes(key + 2 * 4);
    ctx->state[7] = pack_4bytes(key + 3 * 4);
    ctx->state[8] = pack_4bytes(key + 4 * 4);
    ctx->state[9] = pack_4bytes(key + 5 * 4);
    ctx->state[10] = pack_4bytes(key + 6 * 4);
    ctx->state[11] = pack_4bytes(key + 7 * 4);
    ctx->state[12] = 0; // 64 bit counter initialized to zero by default.
    ctx->state[13] = pack_4bytes(nonce + 0 * 4);
    ctx->state[14] = pack_4bytes(nonce + 1 * 4);
    ctx->state[15] = pack_4bytes(nonce + 2 * 4);

    memcpy(ctx->nonce, nonce, sizeof(ctx->nonce));
}

static void chacha20_block_set_counter(struct chacha20_context *ctx, uint64_t counter) {
    ctx->state[12] = (uint32_t) counter;
    ctx->state[13] = pack_4bytes(ctx->nonce + 0 * 4) + (uint32_t) (counter >> 32);
}

static void chacha20_block_next(struct chacha20_context *ctx) {
    /* Mix the bytes a lot and hope that nobody finds out how to undo it. */
    for (int i = 0; i < 16; i++) ctx->key_stream[i] = ctx->state[i];

#define CHACHA20_QUARTERROUND(x, a, b, c, d) \
    x[a] += (x)[b]; (x)[d] = rotl_32((x)[d] ^ (x)[a], 16); \
    (x)[c] += (x)[d]; (x)[b] = rotl_32((x)[b] ^ (x)[c], 12); \
    (x)[a] += (x)[b]; (x)[d] = rotl_32((x)[d] ^ (x)[a], 8); \
    (x)[c] += (x)[d]; (x)[b] = rotl_32((x)[b] ^ (x)[c], 7);

    for (int i = 0; i < 10; i++) {
        CHACHA20_QUARTERROUND(ctx->key_stream, 0, 4, 8, 12)
        CHACHA20_QUARTERROUND(ctx->key_stream, 1, 5, 9, 13)
        CHACHA20_QUARTERROUND(ctx->key_stream, 2, 6, 10, 14)
        CHACHA20_QUARTERROUND(ctx->key_stream, 3, 7, 11, 15)
        CHACHA20_QUARTERROUND(ctx->key_stream, 0, 5, 10, 15)
        CHACHA20_QUARTERROUND(ctx->key_stream, 1, 6, 11, 12)
        CHACHA20_QUARTERROUND(ctx->key_stream, 2, 7, 8, 13)
        CHACHA20_QUARTERROUND(ctx->key_stream, 3, 4, 9, 14)
    }

    for (int i = 0; i < 16; i++) ctx->key_stream[i] += ctx->state[i];

    uint32_t *counter = ctx->state + 12;
    counter[0]++;
    if (0 == counter[0]) {
        counter[1]++;
        assert(0 != counter[1]);
    }
}

void chacha20_init_context(struct chacha20_context *ctx, uint8_t nonce[], uint8_t key[], uint64_t counter) {
    memset(ctx, 0, sizeof(struct chacha20_context));

    chacha20_init_block(ctx, key, nonce);
    chacha20_block_set_counter(ctx, counter);

    ctx->position = 64;
}

void chacha20_xor(struct chacha20_context *ctx, uint8_t *bytes, size_t n_bytes) {
    uint8_t *key_stream = (uint8_t *) ctx->key_stream;
    for (size_t i = 0; i < n_bytes; i++) {
        if (ctx->position >= 64) {
            chacha20_block_next(ctx);
            ctx->position = 0;
        }
        bytes[i] ^= key_stream[ctx->position];
        ctx->position++;
    }
}
