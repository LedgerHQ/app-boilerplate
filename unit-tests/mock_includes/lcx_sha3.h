#ifndef LCX_SHA3_H
#define LCX_SHA3_H

#include <stddef.h>

#include "lcx_hash.h"

#define CX_SHA3_224_SIZE 28
#define CX_SHA3_256_SIZE 32
#define CX_SHA3_384_SIZE 48
#define CX_SHA3_512_SIZE 64

typedef struct {
    cx_hash_t header;
    size_t output_size;
    size_t block_size;
    size_t blen;
    uint8_t block[200];
    uint64bits_t acc[25];
} cx_sha3_t;

static inline cx_err_t cx_sha3_init_no_throw(cx_sha3_t* hash, size_t size) {
    (void)hash;
    (void)size;
    return CX_OK;
}

#endif // LCX_SHA3_H
