#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#define CHACHA20_NONCEBYTES 12
#define CHACHA20_KEYBYTES 32

struct chacha20_context
{
    uint32_t key_stream[16];
    size_t position;

    uint8_t nonce[CHACHA20_NONCEBYTES];
    uint8_t key[CHACHA20_KEYBYTES];

    uint32_t state[16];
};

unsigned char *generate_random_bytestream(size_t num_bytes);

void chacha20_init_context(struct chacha20_context *ctx, uint8_t nonce[], uint8_t key[], uint64_t counter);

void chacha20_xor(struct chacha20_context *ctx, uint8_t *bytes, size_t n_bytes);
