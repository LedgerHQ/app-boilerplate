#pragma once

#include <stdint.h>   // uint*_t
#include <stdbool.h>  // bool

#include "buffer.h"

/**
 * Maximum chunk size accepted for transaction data (bytes).
 * Should match the Python client chunk size constant.
 */
#define MAX_SIGN_TX_CHUNK_SIZE 240

/**
 * Transaction buffer size for dynamic allocation (bytes).
 * Note: Must be less than SIZE_MEM_BUFFER in mem.c to account for:
 * - HEAP_HEADER_SIZE (~160 bytes for heap metadata)
 * - Chunk headers (4-8 bytes per allocation)
 * - Alignment requirements (8-byte alignment)
 */
// TODO max cardano tx size is 16K, but our format of non-serialized (not CBOR) tx might be bigger or smaller, needs to be checked
// up to 19K is possible to alloc, but we want this minimal
#include "transaction/tx_constants.h"
/**
 * Handler for SIGN_TX command. If successfully parse BIP32 path
 * and transaction, sign transaction and send APDU response.
 *
 * @see G_context.bip32_path, G_context.tx_info.raw_transaction,
 * G_context.tx_info.signature and G_context.tx_info.v.
 *
 * @param[in,out] cdata
 *   Command data with BIP32 path and raw transaction serialized.
 * @param[in]     p1
 *   P1 parameter indicating chunk type (P1_TX_INIT, P1_TX_DATA_CHUNK, P1_TX_CHUNK_LAST).
 *
 * @return zero or positive integer if success, negative integer otherwise.
 *
 */
void handler_sign_tx(buffer_t *cdata, uint8_t p1);

/**
 * Handler for SIGN_TX AUX_DATA (CVote) APDUs.
 *
 * @param[in,out] cdata
 *   Command data for the CVote aux data APDU.
 * @param[in]     p2
 *   CVote aux data sub-instruction.
 *
 * @return zero or positive integer if success, negative integer otherwise.
 *
 */
void handler_sign_tx_aux_data(buffer_t *cdata, uint8_t p2);

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
void handler_sign_tx_witness(buffer_t *cdata);

/**
 * Finalize witness signing. Called after user approves witness signature.
 * Sends the witness signature back to the client.
 */
void finalize_witness(bool confirm);
