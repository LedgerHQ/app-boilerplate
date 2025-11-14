#pragma once

#include <stdint.h>   // uint*_t
#include <stdbool.h>  // bool

#include "buffer.h"

/**
 * Transaction buffer size for dynamic allocation (bytes).
 * Note: Must be significantly less than SIZE_MEM_BUFFER in mem.c to account for:
 * - HEAP_HEADER_SIZE (~160 bytes for heap metadata)
 * - Chunk headers (4-8 bytes per allocation)
 * - Alignment requirements (8-byte alignment)
 * Maximum tested working size is 14KB from a 24KB pool TODO
 */
#define TX_BUFFER_SIZE (14 * 1024)

/**
 * Handler for SIGN_TX command. If successfully parse BIP32 path
 * and transaction, sign transaction and send APDU response.
 *
 * @see G_context.bip32_path, G_context.tx_info.raw_transaction,
 * G_context.tx_info.signature and G_context.tx_info.v.
 *
 * @param[in,out] cdata
 *   Command data with BIP32 path and raw transaction serialized.
 * @param[in]     chunk
 *   Index number of the APDU chunk.
 * @param[in]       more
 *   Whether more APDU chunk to be received or not.
 *
 * @return zero or positive integer if success, negative integer otherwise.
 *
 */
int handler_sign_tx(buffer_t *cdata, uint8_t chunk, bool more);

/**
 * Handler for SIGN_TX_WITNESS command. Signs transaction hash with witness key.
 * Must be called after transaction is approved (TX_STATE_APPROVED).
 *
 * @param[in,out] cdata
 *   Command data with witness BIP32 path.
 *
 * @return zero or positive integer if success, negative integer otherwise.
 *
 */
int handler_sign_tx_witness(buffer_t *cdata);
