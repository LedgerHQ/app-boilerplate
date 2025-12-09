/*****************************************************************************
 *   Ledger App Boilerplate.
 *   (c) 2025 Ledger SAS.
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

/**
 * @file provide_token_info.c
 * @brief Handler for PROVIDE_TOKEN_INFO (INS 0x22) - CAL Dynamic Token Support
 *
 * ADVANCED FEATURE - Read before implementing:
 *
 * This demonstrates runtime token metadata updates via CAL (Crypto Asset List, a Ledger HSM
 * trusted backend) signed descriptors with PKI signature validation.
 * It's an advanced token support method.
 */

#include <string.h>  // explicit_bzero, strncpy

#include "os.h"
#include "provide_token_info.h"
#include "dynamic_token_info.h"
#include "tlv_use_case_dynamic_descriptor.h"
#include "io.h"
#include "sw.h"
#include "macros.h"
#include "constants.h"

/**
 * # The Dynamic Token use_case
 *
 * The Dynamic Token use_case is a common format for Ledger applications that want to handle dynamic
 * token reception.
 *
 * As the TLV format is specified and consistent, a dedicated use case has been created in the SDK:
 * tlv_use_case_dynamic_descriptor().
 * We will simply call it with the received TLV payload and use it's output.
 *
 * Though the TLV format is specified and unchanging, a wildcard field, the TUID
 * (Token Unique IDentifier) is application specific. It is recommended to make it a TLV.
 */

/**
 * # TUID TLV tag definitions for boilerplate app
 *
 * For boilerplate, we use as an example a simple TUID with only a 32-bytes token address.
 * Tag 0x10: TOKEN_ADDRESS - 32-byte token contract address
 *
 * Real applications with real tokens will often need more tags: Solana for example expects
 * (token standard, mint address, and 2022 extension codes),
 * Boilerplate keeps it simple with just the address.
 * Unknown tags are REJECTED (strict validation).
 */

/**
 * # Parsing the TUID TLV with the TLV library from the SDK
 *
 * We will use the TLV library of the SDK to automate most of the TLV parsing.
 * This also conveniently serves as an example of the TLV API of the SDK.
 *
 * The usage follows the pattern:
 *     - Create a list of TAG + tlv_handler_cb_t + tag_unicity_t, using the X-Macro format
 *     - Call DEFINE_TLV_PARSER to automatically create the TLV parser function
 *     - Call the crafted function in your C code
 *
 * Please refer to the lib_tlv/tlv_library.h API in the SDK for a more complete documentation.
 */

/**
 * @brief TUID data structure for parsing
 *
 * The output structure of the TUID TLV parser that will be filled by our handlers
 */
typedef struct tlv_TUID_data_s {
    TLV_reception_t received_tags;
    buffer_t token_address_buffer;
} tlv_TUID_data_t;

/**
 * @brief Handler for TOKEN_ADDRESS tag in TUID
 *
 * Function is of tlv_handler_cb_t type
 */
static bool handle_tuid_token_address(const tlv_data_t *data, tlv_TUID_data_t *tlv_TUID_data) {
    PRINTF("Handling TUID TOKEN_ADDRESS tag\n");
    // Token address must be exactly 32 bytes (TOKEN_ADDRESS_LEN)
    return get_buffer_from_tlv_data(data,
                                    // output of the helper
                                    &tlv_TUID_data->token_address_buffer,
                                    // Min address length
                                    TOKEN_ADDRESS_LEN,
                                    // Max address length
                                    TOKEN_ADDRESS_LEN);
}

// clang-format off
#define TUID_TLV_TAGS(X)                                                            \
    X(0x10, TAG_TOKEN_ADDRESS,       handle_tuid_token_address, ENFORCE_UNIQUE_TAG) \
  //X(0x11, TAG_EXAMPLE_SOMETHING,   handle_xyz,                ENFORCE_UNIQUE_TAG) \
  //X(0x12, TAG_ANOTHER_ONE,         handle_abc,                ENFORCE_UNIQUE_TAG) \
  //X(0x13, TAG_ANOTHER_ANOTHER_ONE, handle_abc,                ENFORCE_UNIQUE_TAG) \
// clang-format on

// Use SDK macro to generate TUID parser function
// This creates:
// parse_dynamic_token_tuid(const buffer_t *payload, void *tlv_out, TLV_reception_t *received)
// The parser strictly validates that only defined tags are present
DEFINE_TLV_PARSER(TUID_TLV_TAGS, NULL, parse_dynamic_token_tuid)

/**
 * @brief Validate coin type matches boilerplate's SLIP-44 value
 *
 * Accepts both non-hardened (0x8001) and hardened (0x80008001) forms.
 *
 * @param[in] coin_type Coin type from TLV descriptor
 * @return true if coin type is valid
 */
static bool validate_coin_type(uint32_t coin_type) {
    return coin_type == BOILERPLATE_SLIP44_COIN_TYPE ||
           coin_type == BOILERPLATE_SLIP44_COIN_TYPE_HARDENED;
}

int handler_provide_token_info(buffer_t *cdata) {
    PRINTF("=== PROVIDE_TOKEN_INFO handler ===\n");

    // Reset dynamic token storage to ensure clean state
    init_dynamic_token_storage();

    // Parse and validate TLV descriptor using SDK function
    // This performs:
    //   1. TLV structure parsing (all 8 tags)
    //   2. Signature verification via PKI (CERTIFICATE_PUBLIC_KEY_USAGE_COIN_META)
    //   3. Version check
    //   4. Application name validation
    tlv_dynamic_descriptor_out_t tlv_output = {0};
    tlv_dynamic_descriptor_status_t status = tlv_use_case_dynamic_descriptor(cdata, &tlv_output);

    if (status != TLV_DYNAMIC_DESCRIPTOR_SUCCESS) {
        PRINTF("Failed to parse TLV descriptor: status=0x%x\n", status);
        return io_send_sw(SW_INVALID_DYNAMIC_TOKEN);
    }

    PRINTF("TLV descriptor parsed successfully\n");
    PRINTF("  Version: %d\n", tlv_output.version);
    PRINTF("  Coin type: 0x%08x\n", tlv_output.coin_type);
    PRINTF("  Ticker: %s\n", tlv_output.ticker);
    PRINTF("  Magnitude (decimals): %d\n", tlv_output.magnitude);

    // Validate coin type matches SLIP-44 value
    if (!validate_coin_type(tlv_output.coin_type)) {
        PRINTF("Invalid coin type - expected 0x%08x or 0x%08x, got 0x%08x\n",
               BOILERPLATE_SLIP44_COIN_TYPE,
               BOILERPLATE_SLIP44_COIN_TYPE_HARDENED,
               tlv_output.coin_type);
        return io_send_sw(SW_INVALID_DYNAMIC_TOKEN);
    }

    // Parse TUID (Token Unique IDentifier) sub-TLV
    // We call the function crafted by the Macro
    tlv_TUID_data_t tlv_TUID_data = {0};
    if (!parse_dynamic_token_tuid(&tlv_output.TUID, &tlv_TUID_data, &tlv_TUID_data.received_tags)) {
        PRINTF("Failed to parse TUID sub-TLV\n");
        return io_send_sw(SW_INVALID_DYNAMIC_TOKEN);
    }

    // Verify that the required TAG_TOKEN_ADDRESS was received in the TUID
    if (!TLV_CHECK_RECEIVED_TAGS(tlv_TUID_data.received_tags, TAG_TOKEN_ADDRESS)) {
        PRINTF("Error: missing required TOKEN_ADDRESS in TUID\n");
        return io_send_sw(SW_INVALID_DYNAMIC_TOKEN);
    }

    // Copy validated token metadata to global storage
    set_token_info(tlv_output.magnitude, &tlv_output.ticker, &tlv_TUID_data.token_address_buffer);

    return io_send_sw(SWO_SUCCESS);
}
