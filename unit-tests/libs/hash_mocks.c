// Hash mock implementations for unit testing
// Uses reference implementations of Blake2b and SHA3-256

#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "blake2b.h"
#include "hash.h"
#include "sha3-256.h"

/**
 * Initialize Blake2b context with specified output length
 */
cx_err_t cx_blake2b_init_no_throw(cx_blake2b_t *ctx, uint8_t output_len) {
    if (output_len < 1 || output_len > 64) {
        return -1;  // Invalid output length
    }

    memset(ctx, 0, sizeof(cx_blake2b_t));
    ctx->header.algo = CX_BLAKE2B;
    ctx->header.counter = 0;
    ctx->output_len = output_len;
    ctx->buffer_len = 0;
    ctx->total_len = 0;

    // Initialize the Blake2b internal state stored in the context's buffer
    blake2b_t *blake2b_state = &ctx->state;
    if (blake2b_init(blake2b_state, output_len) != 0) {
        return -1;
    }

    return CX_OK;
}

/**
 * Add more data to hash or finalize
 * Mode: 0 = update, CX_LAST (0x80000000) = finalize
 */
cx_err_t cx_hash_no_throw(cx_hash_t *hash, int mode,
                          const uint8_t *in, size_t in_len,
                          uint8_t *out, size_t out_len) {
    if (hash == NULL) {
        return -1;
    }

    // Cast to Blake2b context
    cx_blake2b_t *ctx = (cx_blake2b_t *)hash;
    blake2b_t *blake2b_state = &ctx->state;

    // Check if state needs initialization
    // If output_len is set but algo is 0 (uninitialized), initialize Blake2b state
    if (ctx->output_len > 0 && ctx->header.algo == 0) {
        // Initialize the Blake2b state in the context's buffer
        if (blake2b_init(blake2b_state, ctx->output_len) != 0) {
            return -1;
        }
        // Mark as initialized
        ctx->header.algo = CX_BLAKE2B;
    }

    // Verify context is Blake2b type
    if (ctx->header.algo != CX_BLAKE2B) {
        return -1;
    }

    // Handle update mode (no finalize flag set)
    if ((mode & CX_LAST) == 0) {
        // Just accumulate data
        if (in != NULL && in_len > 0) {
            if (blake2b_update(blake2b_state, in, in_len) != 0) {
                return -1;
            }
            ctx->total_len += in_len;
        }
        return CX_OK;
    }

    // Handle finalize mode (CX_LAST)
    uint8_t final_output[64] = {0};

    // If there's input data in finalize call, update first
    if (in != NULL && in_len > 0) {
        if (blake2b_update(blake2b_state, in, in_len) != 0) {
            return -1;
        }
        ctx->total_len += in_len;
    }

    // Finalize and get output
    if (blake2b_final(blake2b_state, final_output, ctx->output_len) != 0) {
        return -1;
    }

    // Copy result to output buffer if provided
    if (out != NULL && out_len >= ctx->output_len) {
        memcpy(out, final_output, ctx->output_len);
    } else if (out != NULL) {
        return -1;  // Output buffer too small
    }

    return CX_OK;
}
