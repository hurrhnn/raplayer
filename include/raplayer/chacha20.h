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

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#ifndef RAPLAYER_CHACHA20_H
#define RAPLAYER_CHACHA20_H

#define CHACHA20_NONCEBYTES 12
#define CHACHA20_KEYBYTES 32

struct chacha20_context
{
    uint32_t key_stream[16];
    size_t position;

    uint8_t nonce[CHACHA20_NONCEBYTES];
    uint8_t key[CHACHA20_KEYBYTES];

    uint32_t state[16];
} __attribute__((aligned(1), packed));

unsigned char *generate_random_bytestream(size_t num_bytes);

void chacha20_init_context(struct chacha20_context *ctx, uint8_t nonce[], uint8_t key[], uint64_t counter);

void chacha20_xor(struct chacha20_context *ctx, uint8_t *bytes, size_t n_bytes);

#endif