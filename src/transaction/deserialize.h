#pragma once

#include "buffer.h"

#include "types.h"

/**
 * Deserialize raw transaction in structure.
 *
 * @param[in, out] buf
 *   Pointer to buffer with serialized transaction.
 * @param[out]     tx
 *   Pointer to transaction structure.
 * @param[in]  is_token_transaction
 *  Whether the transaction to deserialize is a token transaction or not.
 *
 * @return PARSING_OK if success, error status otherwise.
 *
 */
parser_status_e transaction_deserialize(buffer_t *buf,
                                        transaction_t *tx,
                                        bool is_token_transaction);
