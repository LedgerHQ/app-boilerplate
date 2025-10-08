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
    OUTPUT_ADDRESS_SIZE_ERROR = -14
} parser_status_e;

/**
 * Deserialize raw transaction in structure.
 *
 * @param[in, out] buf
 *   Pointer to buffer with serialized transaction.
 * @param[out]     tx
 *   Pointer to transaction structure.
 *
 * @return PARSING_OK if success, error status otherwise.
 *
 */
parser_status_e transaction_deserialize(buffer_t *buf, transaction_t *tx);
