#pragma once

#include <stdint.h>
#include <stddef.h>

#include "buffer.h"

/**
 * ML-DSA APDU handler for key generation, signing, and verification operations.
 *
 * Supports:
 * - ML-DSA-44 (INS=0x40, P2 bit 0-1: 0x00)
 * - ML-DSA-65 (INS=0x40, P2 bit 0-1: 0x01)
 * - ML-DSA-87 (INS=0x40, P2 bit 0-1: 0x02)
 *
 * Uses chunked APDU protocol for large payloads.
 * Implements sign_mu/verify_mu interface using MLDSA_sign_prehash/MLDSA_verify_prehash
 * with SHA3-512 as the prehash algorithm.
 *
 * @param[in] cdata      Buffer containing APDU command data
 * @param[in] ins        Instruction code (0x40=KEYGEN, 0x41=SIGN, 0x42=VERIFY)
 * @param[in] chunk_idx  Chunk index from P1
 * @param[in] p2         P2 byte (bits 0-1: param_set, bit 7: more flag)
 *
 * @return zero or positive integer if success, negative integer otherwise.
 */
int handler_mldsa(buffer_t *cdata, uint8_t ins, uint8_t chunk_idx, uint8_t p2);
