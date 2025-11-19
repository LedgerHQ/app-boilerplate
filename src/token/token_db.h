#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "types.h"

/**
 * Get token information from the hardcoded database.
 *
 * @param[in]  token_address  32-byte token address to lookup
 * @param[out] token_info     pointer to token_info_t to fill
 *
 * @return true if token found, false otherwise
 */
bool get_token_info(const uint8_t *token_address, token_info_t *token_info);
