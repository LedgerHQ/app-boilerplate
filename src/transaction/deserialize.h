#pragma once

#include "buffer.h"

#include "types.h"

typedef enum {
    PARSING_OK = 1,
    TO_PARSING_ERROR = -2,
    VALUE_PARSING_ERROR = -3,
    FEE_PARSING_ERROR = -4,
    WRONG_LENGTH_ERROR = -8,
    INPUTS_COUNT_PARSING_ERROR = -9,
    INPUTS_PARSING_ERROR = -10,
    OUTPUTS_COUNT_PARSING_ERROR = -11,
    OUTPUTS_PARSING_ERROR = -12,
    OUTPUT_DESTINATION_TYPE_ERROR = -13,
    OUTPUT_ADDRESS_SIZE_ERROR = -14,
    WITHDRAWALS_PARSING_ERROR = -15
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
parser_status_e transaction_deserialize(buffer_t *buf, transaction_t *tx);

/**
 * Cleanup all dynamically allocated structures in transaction.
 *
 * Frees all allocated transaction elements (inputs, outputs, withdrawals, etc.) and their
 * nested allocations (asset groups/tokens, inline datums, reference scripts). Safe to
 * call multiple times or on partially-initialized transactions.
 *
 * USAGE CONTRACT:
 * - Call this after transaction_deserialize() on ALL code paths (success and error)
 * - Call this before deallocating the transaction_t structure itself
 * - Call this in any error path that returns early from transaction processing
 *
 * @param[in, out] tx
 *   Pointer to transaction structure with element lists to cleanup. Structure itself is not freed.
 */
void tx_context_cleanup(transaction_t *tx);
