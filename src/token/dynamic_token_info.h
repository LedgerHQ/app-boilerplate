/*****************************************************************************
 *   Ledger App Boilerplate.
 *   (c) 2020 Ledger SAS.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *****************************************************************************/

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "types.h"
#include "tokens.h"
#include "buffer.h"

/**
 * @brief Initialize dynamic token storage
 *
 * Sets the received flag to false, indicating no dynamic token info is loaded.
 * Should be called at app startup.
 */
void init_dynamic_token_storage(void);

/**
 * @brief Copy validated token metadata to global storage
 *
 * Copies ticker, decimals, and address from TLV output to g_dynamic_token_info.
 * Ticker is null-terminated for safety.
 *
 * @param[in] decimals Token decimals from TLV descriptor
 * @param[in] ticker Token ticker from TLV descriptor
 * @param[in] token_address_buffer Buffer containing 32-byte token address
 */
void set_token_info(uint8_t decimals,
                    char (*ticker)[MAX_TICKER_SIZE + 1],
                    const buffer_t *token_address_buffer);

/**
 * Get token information from token database (dynamic CAL + hardcoded).
 *
 * Lookup priority:
 *   1. Dynamic tokens from CAL (if provided via PROVIDE_TOKEN_INFO)
 *   2. Hardcoded database in token_db.c (TOKENS[] array)
 *
 * @param[in]  token_address  32-byte token address to lookup
 * @param[out] token_info     pointer to token_info_t to fill
 *
 * @return true if token found, false otherwise
 */
bool get_token_info(const uint8_t *token_address, token_info_t *token_info);
