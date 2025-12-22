#pragma once

#include "buffer.h"

#include "transaction/tx.h"

typedef enum {
    PARSING_OK = 1,
    FEE_PARSING_ERROR = -4,
    TX_SIZE_TOO_LARGE_ERROR = -8,
    INPUTS_COUNT_PARSING_ERROR = -9,
    INPUTS_PARSING_ERROR = -10,
    OUTPUTS_COUNT_PARSING_ERROR = -11,
    OUTPUTS_PARSING_ERROR = -12,
    OUTPUT_DESTINATION_TYPE_ERROR = -13,
    OUTPUT_ADDRESS_SIZE_ERROR = -14,
    WITHDRAWALS_PARSING_ERROR = -15,
    TTL_PARSING_ERROR = -16,
    VALIDITY_INTERVAL_START_PARSING_ERROR = -17,
    CERTIFICATES_PARSING_ERROR = -19,
    TX_BUFFER_NOT_FULLY_CONSUMED_ERROR = -18
} parser_status_e;

/**
 * Deserialize raw transaction buffer into structured format.
 *
 * Allocates memory for transaction elements (inputs, outputs, withdrawals, etc.) and their
 * nested structures (asset groups, tokens, inline datums, reference scripts). On parsing
 * failure, some allocations may be partially completed. The caller MUST call
 * tx_context_cleanup() on both success and failure to ensure all allocated memory is freed.
 *
 * OWNERSHIP MODEL:
 * - This function allocates memory for parsed structures
 * - Caller owns the responsibility for cleanup via tx_context_cleanup()
 * - Must be called on every code path: both on success and all error returns
 *
 * @param[in, out] buf
 *   Pointer to buffer with serialized transaction.
 * @param[out]     tx
 *   Pointer to transaction structure (caller-owned). Populated with pointers to
 *   allocated element lists on success, or partially populated on error.
 *
 * @return PARSING_OK if success, error status otherwise.
 *
 * @see tx_context_cleanup
 */
parser_status_e parse_tx(buffer_t *buf, transaction_t *tx);

int tx_handle_parse_error(parser_status_e status);

/**
 * Cleanup all dynamically allocated structures in transaction.
 *
 * Operates on G_context.tx_info.transaction. Frees all allocated transaction elements
 * (inputs, outputs, withdrawals, certificates, etc.) and their nested allocations (asset groups/tokens,
 * inline datums, reference scripts). Safe to call multiple times or on partially-initialized
 * transactions.
 *
 * USAGE CONTRACT:
 * - Call this after parse_tx() on ALL code paths (success and error)
 * - Call this in any error path that returns early from transaction processing
 */
void tx_context_cleanup(void);
