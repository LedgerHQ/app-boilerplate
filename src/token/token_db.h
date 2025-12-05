#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "types.h"

/**
 * @brief Search for token in hardcoded database
 *
 * @param[in] token_address 32-byte token address to look up
 * @param[out] token_info Output structure to populate with ticker and decimals
 * @return true if token found in database
 * @return false if token not found
 */
bool get_hardcoded_token_info(const uint8_t *token_address, token_info_t *token_info);
