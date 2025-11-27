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

#include <string.h>  // explicit_bzero

#include "os.h"
#include "token_db.h"
#include "tlv_library.h"
#include "macros.h"
#include "dynamic_token_info.h"

/**
 * @brief Structure to store dynamic token information received from CAL (Crypto Asset List)
 *
 * This structure holds token metadata provided via a TLV-encoded descriptor signed by Ledger's
 * HSM CAL (Crypto Asset List) trusted backend.
 * Dynamic tokens override the hardcoded token database and persist in RAM until app exit.
 * This application chooses to manage only one token info slot.
 *
 * PRODUCTION NOTE: Requires knowledge of your token by the CAL service from Ledger
 * Most third-party apps should probably use the hardcoded token database instead (see token_db.c).
 */
typedef struct dynamic_token_info_s {
    bool received;                       ///< True if valid TLV descriptor received and verified
    uint8_t address[TOKEN_ADDRESS_LEN];  ///< 32-byte token address extracted from TUID
    char ticker[MAX_TICKER_SIZE + 1];    ///< Token ticker (e.g., "USDC") from outer TLV
    uint8_t decimals;                    ///< Number of decimals from magnitude field
} dynamic_token_info_t;

dynamic_token_info_t g_dynamic_token_info;

void init_dynamic_token_storage(void) {
    explicit_bzero(&g_dynamic_token_info, sizeof(g_dynamic_token_info));
}

void set_token_info(uint8_t decimals,
                    char (*ticker)[MAX_TICKER_SIZE + 1],
                    const buffer_t *token_address_buffer) {
    // Copy token address
    memcpy(g_dynamic_token_info.address, token_address_buffer->ptr, TOKEN_ADDRESS_LEN);

    // Copy ticker with null termination (SDK validates ≤ MAX_TICKER_SIZE)
    strncpy(g_dynamic_token_info.ticker, *ticker, sizeof(g_dynamic_token_info.ticker) - 1);
    g_dynamic_token_info.ticker[sizeof(g_dynamic_token_info.ticker) - 1] = '\0';

    // Copy decimals
    g_dynamic_token_info.decimals = decimals;

    // Mark as received - enables dynamic token lookup
    g_dynamic_token_info.received = true;

    PRINTF("Dynamic token stored successfully:\n");
    PRINTF("  Address: %.*H\n", TOKEN_ADDRESS_LEN, g_dynamic_token_info.address);
    PRINTF("  Ticker: %s\n", g_dynamic_token_info.ticker);
    PRINTF("  Decimals: %d\n", g_dynamic_token_info.decimals);
}

/**
 * @brief If we have received knowledge from CAL about a token, return it.
 *
 * Dynamic tokens from CAL (Crypto Asset List) take priority over the hardcoded
 * database, allowing runtime token metadata updates without app upgrades.
 *
 * The dynamic token persists in RAM until app exit.
 *
 * @param[in] token_address 32-byte token address to look up
 * @param[out] token_info Output structure to populate with ticker and decimals
 * @return true if dynamic token is loaded and address matches
 * @return false if no dynamic token or address doesn't match
 */
static bool get_dynamic_token_info(const uint8_t *token_address, token_info_t *token_info) {
    // Check if dynamic token has been provided via PROVIDE_TOKEN_INFO
    if (!g_dynamic_token_info.received) {
        PRINTF("No dynamic token info received from CAL\n");
        return false;
    }

    // Compare token address (32 bytes)
    if (memcmp(token_address, g_dynamic_token_info.address, TOKEN_ADDRESS_LEN) != 0) {
        PRINTF("Requested dynamic token address '%.*H' does not match received address '%.*H'\n",
               TOKEN_ADDRESS_LEN,
               token_address,
               TOKEN_ADDRESS_LEN,
               g_dynamic_token_info.address);
        return false;
    }

    // Match found - return dynamic token info (zero-copy the ticker)
    token_info->ticker = g_dynamic_token_info.ticker;
    token_info->decimals = g_dynamic_token_info.decimals;

    PRINTF("Using dynamic token from CAL: %s (decimals: %d)\n",
           token_info->ticker,
           token_info->decimals);

    return true;
}

bool get_token_info(const uint8_t *token_address, token_info_t *token_info) {
    if (token_address == NULL || token_info == NULL) {
        return false;
    }

    // Priority 1: Check dynamic token from CAL first
    if (get_dynamic_token_info(token_address, token_info)) {
        PRINTF("Found token in dynamic CAL database\n");
        return true;
    }

    // Priority 2: Fall back to hardcoded token database
    if (get_hardcoded_token_info(token_address, token_info)) {
        PRINTF("Found token in hardcoded database\n");
        return true;
    }

    return false;
}
