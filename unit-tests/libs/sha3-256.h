// SHA3-256 reference implementation
// Based on public domain Keccak code

#ifndef SHA3_256_H
#define SHA3_256_H

#include <stdint.h>
#include <string.h>

#define SHA3_256_HASH_SIZE 32

/**
 * @brief Compute SHA3-256 hash
 * @param[in] data Input data
 * @param[in] len Input length in bytes
 * @param[out] hash Output hash (32 bytes)
 */
void calc_sha3_256(uint8_t *hash, const uint8_t *data, size_t len);

#endif  // SHA3_256_H
