#pragma once

#include "buffer.h"

#include "opcert/opcert_types.h"

typedef enum {
    PARSING_OK = 1,
    KES_PUBLIC_KEY_PARSING_ERROR = -1,
    KES_PERIOD_PARSING_ERROR = -2,
    ISSUE_COUNTER_PARSING_ERROR = -3,
    POOL_COLD_KEY_PATH_PARSING_ERROR = -4,
    WRONG_LENGTH_ERROR = -7
} opcert_parser_status_e;

/**
 * Deserialize opcert.
 *
 * @param[in, out] buf
 *   Pointer to buffer with serialized transaction.
 * @param[out]     opcert
 *   Pointer to opcert structure.
 *
 * @return PARSING_OK if success, error status otherwise.
 *
 */
opcert_parser_status_e parse_opcert(buffer_t *buf, parsed_opcert_t *opcert);
