// BLAKE2b reference implementation
// From https://github.com/BLAKE2/BLAKE2

#ifndef BLAKE2B_H
#define BLAKE2B_H

#include <stdint.h>
#include <string.h>

typedef struct {
    uint64_t h[8];
    uint64_t t[2];
    uint64_t f[2];
    uint8_t  buf[128];
    size_t   buflen;
    uint8_t  outlen;
    uint8_t  last_node;
} blake2b_state;

typedef blake2b_state blake2b_t;

/**
 * @brief Initialize BLAKE2b context
 * @param S Blake2b state
 * @param outlen Output length (1-64)
 * @return 0 on success
 */
int blake2b_init(blake2b_t *S, uint8_t outlen);

/**
 * @brief Update BLAKE2b with data
 * @param S Blake2b state
 * @param in Input data
 * @param inlen Input length
 * @return 0 on success
 */
int blake2b_update(blake2b_t *S, const void *in, size_t inlen);

/**
 * @brief Finalize BLAKE2b and produce output
 * @param S Blake2b state
 * @param out Output buffer
 * @param outlen Output length
 * @return 0 on success
 */
int blake2b_final(blake2b_t *S, void *out, uint8_t outlen);

/**
 * @brief One-shot BLAKE2b hash
 * @param out Output buffer
 * @param outlen Output length (1-64)
 * @param in Input data
 * @param inlen Input length
 * @return 0 on success
 */
int blake2b(void *out, size_t outlen, const void *in, size_t inlen);

/**
 * @brief Convenience function for BLAKE2b-256
 */
static inline void blake2b_256(uint8_t *out, const uint8_t *in, size_t inlen) {
    blake2b(out, 32, in, inlen);
}

/**
 * @brief Convenience function for BLAKE2b-224
 */
static inline void blake2b_224(uint8_t *out, const uint8_t *in, size_t inlen) {
    blake2b(out, 28, in, inlen);
}

#endif  // BLAKE2B_H
