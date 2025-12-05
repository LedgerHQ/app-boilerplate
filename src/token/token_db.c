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

#include <string.h>  // memcmp, strncpy

#include "os.h"
#include "tokens.h"
#include "macros.h"
#include "token_db.h"

// Structure for token database entries: maps 32-byte address to ticker and decimals
typedef struct token_entry_s {
    uint8_t address[TOKEN_ADDRESS_LEN];
    char ticker[MAX_TICKER_SIZE + 1];
    uint8_t decimals;
} token_entry_t;

/**
 * Hardcoded token knowledge database.
 * Maps 32-byte token addresses to tickers and decimals.
 *
 * All values are examples as this blockchain does not exist, it's example showcase.
 *
 * THIRD-PARTY DEVELOPERS: You can freely modify this database for your tokens.
 * No Ledger coordination required - just add your token entries and rebuild.
 *
 * For SWAP support with these tokens, submit them to Ledger's CAL database (quick/easy).
 * For runtime-updated tokens, see PROVIDE_TOKEN_INFO handler (requires PKI coordination).
 */
static const token_entry_t TOKENS[] = {
    // Example Token 1: USDC-like token with 12 decimals
    {.address = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23, 0x45,
                 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab,
                 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef},
     .ticker = "USDC",
     .decimals = 12},

    // Example Token 2: WETH-like token with 14 decimals
    {.address = {0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10, 0xfe, 0xdc, 0xba,
                 0x98, 0x76, 0x54, 0x32, 0x10, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54,
                 0x32, 0x10, 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10},
     .ticker = "WETH",
     .decimals = 14},

    // Example Token 3: DAI-like token with 14 decimals
    {.address = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11, 0x22, 0x33, 0x44,
                 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
                 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99},
     .ticker = "DAI",
     .decimals = 14},

    // Example Token 4: Short ticker with 12 decimals
    {.address = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb,
                 0xcc, 0xdd, 0xee, 0xff, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66,
                 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00},
     .ticker = "BTC",
     .decimals = 12},

    // Example Token 5: Another short ticker with 14 decimals
    {.address = {0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, 0x77, 0x66, 0x55,
                 0x44, 0x33, 0x22, 0x11, 0x00, 0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa,
                 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00},
     .ticker = "LINK",
     .decimals = 14}};

bool get_hardcoded_token_info(const uint8_t *token_address, token_info_t *token_info) {
    for (uint8_t i = 0; i < ARRAY_LENGTH(TOKENS); i++) {
        if (memcmp(token_address, TOKENS[i].address, TOKEN_ADDRESS_LEN) == 0) {
            // Found the token - zero-copy assignment
            token_info->ticker = TOKENS[i].ticker;
            token_info->decimals = TOKENS[i].decimals;
            return true;
        }
    }
    return false;
}
