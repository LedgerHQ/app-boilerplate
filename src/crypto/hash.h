#pragma once

#include "cx.h"

#include "utils/utils.h"

// This file provides convenience functions for using firmware hashing api

// Note: We would like to make this static const but
// it does not play well with inline functions
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

#define __CIPHER_DECLARE(CIPHER, cipher, bits)                                                  \
    typedef struct {                                                                            \
        uint16_t initialized_magic;                                                             \
        cx_##cipher##_t cx_ctx;                                                                 \
    } cipher##_##bits##_context_t;                                                              \
                                                                                                \
    static __attribute__((always_inline, unused)) void cipher##_##bits##_init(                  \
        cipher##_##bits##_context_t* ctx) {                                                     \
        STATIC_ASSERT(bits == CIPHER##_##bits##_SIZE * 8, "bad cipher size");                   \
        cx_err_t error = cx_##cipher##_init_no_throw(&ctx->cx_ctx, CIPHER##_##bits##_SIZE * 8); \
        if (error != CX_OK) {                                                                   \
            PRINTF("error: %d", error);                                                         \
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
            PRINTF("error: %d", error);                                                         \
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
            PRINTF("error: %d", error);                                                         \
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
        /* Note: This could be done by single cx_hash call */                                   \
        /* But we don't really care */                                                          \
        cipher##_##bits##_append(&ctx, inBuffer, inSize);                                       \
        cipher##_##bits##_finalize(&ctx, outBuffer, outSize);                                   \
    }

__CIPHER_DECLARE(BLAKE2B, blake2b, 160)
__CIPHER_DECLARE(BLAKE2B, blake2b, 224)
__CIPHER_DECLARE(BLAKE2B, blake2b, 256)
__CIPHER_DECLARE(BLAKE2B, blake2b, 512)

__CIPHER_DECLARE(SHA3, sha3, 256)
