#pragma once

#include <stdint.h>
#include <stddef.h>

#include "buffer.h"

/**
 * ML-KEM APDU handler for key generation, encapsulation, and decapsulation.
 *
 * Supports:
 * - ML-KEM-512  (P2 bits 0-1: 0x00)
 * - ML-KEM-768  (P2 bits 0-1: 0x01)
 * - ML-KEM-1024 (P2 bits 0-1: 0x02)
 *
 * Uses chunked APDU protocol for large payloads.
 *
 * @param[in] cdata      Buffer containing APDU command data
 * @param[in] ins        Instruction code (0x30=KEYGEN, 0x31=ENCAPSULATE, 0x32=DECAPSULATE)
 * @param[in] chunk_idx  Chunk index from P1
 * @param[in] p2         P2 byte (bits 0-1: param_set, bit 7: more flag)
 *
 * @return zero or positive integer if success, negative integer otherwise.
 */
int handler_mlkem(buffer_t *cdata, uint8_t ins, uint8_t chunk_idx, uint8_t p2);
