#pragma once

// Mock hash.h for unit testing
// This mock provides Blake2b and SHA3-256 hashing using reference implementations

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "assert.h"
#include "utils/utils.h"
#include "lcx_hash.h"
#include "ledger_assert.h"

// Mock definitions for compile-time assertions and memory checks
// Note: ASSERT and LEDGER_ASSERT are defined in assert.h/ledger_assert.h
// Note: TRACE and BUFFER_SIZE_PARANOIA are defined in utils.h

#ifdef CX_LAST
#undef CX_LAST
#endif
#define CX_LAST 0x80000000

// Forward declarations for functions defined in hash_mocks.c
cx_err_t cx_blake2b_init_no_throw(cx_blake2b_t *ctx, uint8_t output_len);
cx_err_t cx_hash_no_throw(cx_hash_t *hash, int mode,
                          const uint8_t *in, size_t in_len,
                          uint8_t *out, size_t out_len);

// Hash size constants
enum {
    BLAKE2B_160_SIZE = 20,
    BLAKE2B_224_SIZE = 28,
    BLAKE2B_256_SIZE = 32,
    BLAKE2B_512_SIZE = 64,

    SHA3_256_SIZE = 32,
};

enum {
    HASH_CONTEXT_INITIALIZED_MAGIC = 12345,
};

// Macro to generate hash context types and functions
#define __CIPHER_DECLARE(CIPHER, cipher, bits)                                                  \
    typedef struct {                                                                            \
        uint16_t initialized_magic;                                                             \
        cx_##cipher##_t cx_ctx;                                                                 \
    } cipher##_##bits##_context_t;                                                              \
                                                                                                \
    static __attribute__((always_inline, unused)) void cipher##_##bits##_init(                  \
        cipher##_##bits##_context_t* ctx) {                                                     \
        STATIC_ASSERT(bits == CIPHER##_##bits##_SIZE * 8, "bad cipher size");                   \
        cx_err_t error = cx_##cipher##_init_no_throw(&ctx->cx_ctx, CIPHER##_##bits##_SIZE * 8 / 8); \
        if (error != CX_OK) {                                                                   \
            TRACE("error: %d", error);                                                          \
            ASSERT(false);                                                                      \
        }                                                                                       \
        ctx->initialized_magic = HASH_CONTEXT_INITIALIZED_MAGIC;                                \
    }                                                                                           \
                                                                                                \
    static __attribute__((always_inline, unused)) void cipher##_##bits##_append(                \
        cipher##_##bits##_context_t* ctx,                                                       \
        const uint8_t* inBuffer,                                                                \
        size_t inSize) {                                                                        \
        ASSERT(ctx->initialized_magic == HASH_CONTEXT_INITIALIZED_MAGIC);                       \
        cx_err_t error = cx_hash_no_throw(&ctx->cx_ctx.header,                                  \
                                          0, /* Do not output the hash, yet */                  \
                                          inBuffer,                                             \
                                          inSize,                                               \
                                          NULL,                                                 \
                                          0);                                                   \
        if (error != CX_OK) {                                                                   \
            TRACE("error: %d", error);                                                          \
            ASSERT(false);                                                                      \
        }                                                                                       \
    }                                                                                           \
                                                                                                \
    static __attribute__((always_inline, unused)) void cipher##_##bits##_finalize(              \
        cipher##_##bits##_context_t* ctx,                                                       \
        uint8_t* outBuffer,                                                                     \
        size_t outSize) {                                                                       \
        ASSERT(ctx->initialized_magic == HASH_CONTEXT_INITIALIZED_MAGIC);                       \
        ASSERT(outSize == CIPHER##_##bits##_SIZE);                                              \
        cx_err_t error = cx_hash_no_throw(&ctx->cx_ctx.header,                                  \
                                          CX_LAST, /* Output the hash */                        \
                                          NULL,                                                 \
                                          0,                                                    \
                                          outBuffer,                                            \
                                          CIPHER##_##bits##_SIZE);                              \
        if (error != CX_OK) {                                                                   \
            TRACE("error: %d", error);                                                          \
            ASSERT(false);                                                                      \
        }                                                                                       \
    }                                                                                           \
    /* Convenience function to make all in one step */                                          \
    static __attribute__((always_inline, unused)) void cipher##_##bits##_hash(                  \
        const uint8_t* inBuffer,                                                                \
        size_t inSize,                                                                          \
        uint8_t* outBuffer,                                                                     \
        size_t outSize) {                                                                       \
        ASSERT(inSize < BUFFER_SIZE_PARANOIA);                                                  \
        ASSERT(outSize == CIPHER##_##bits##_SIZE);                                              \
        cipher##_##bits##_context_t ctx;                                                        \
        cipher##_##bits##_init(&ctx);                                                           \
        cipher##_##bits##_append(&ctx, inBuffer, inSize);                                       \
        cipher##_##bits##_finalize(&ctx, outBuffer, outSize);                                   \
    }

__CIPHER_DECLARE(BLAKE2B, blake2b, 160)
__CIPHER_DECLARE(BLAKE2B, blake2b, 224)
__CIPHER_DECLARE(BLAKE2B, blake2b, 256)
__CIPHER_DECLARE(BLAKE2B, blake2b, 512)

// SHA3-256 support requires cx_sha3_t which is not in our mocks
// __CIPHER_DECLARE(SHA3, sha3, 256)

// Simple inline SHA3-256 implementation
static inline void sha3_256_hash(
    const uint8_t* inBuffer,
    size_t inSize,
    uint8_t* outBuffer,
    size_t outSize) {
    // Forward declare the reference implementation function
    extern void calc_sha3_256(uint8_t *hash, const uint8_t *data, size_t len);
    ASSERT(outSize == 32);
    ASSERT(inSize < BUFFER_SIZE_PARANOIA);
    calc_sha3_256(outBuffer, inBuffer, inSize);
}
