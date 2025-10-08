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

#include <stdbool.h>  // bool
#include <string.h>   // memset

#include "os.h"
#include "glyphs.h"
#include "os_io_seproxyhal.h"
#include "nbgl_use_case.h"
#include "io.h"
#include "bip32.h"
#include "format.h"

#include "display.h"
#include "constants.h"
#include "globals.h"
#include "sw.h"
#include "address.h"
#include "tx_types.h"
#include "tx_output_types.h"
#include "tx_warnings.h"
#include "menu.h"
#include "mem.h"
#include "addressUtils/addressUtilsShelley.h"
#include "nbgl_screens.h"
#include "utils/textUtils.h"

// Buffer where the transaction fee string is written
static char g_fee[30];
// Buffer where the transaction TTL string is written
static char g_ttl[30];

// Dynamic arrays for pairs and their string buffers
static nbgl_contentTagValue_t *g_pairs = NULL;
static char **g_pair_values = NULL;
static uint16_t g_num_pairs = 0;

static nbgl_contentTagValueList_t pairList;

// Warning info for network warnings
static char g_warning_msg[128];
static nbgl_contentCenter_t warningInfo;
static nbgl_warningDetails_t warningDetails;
static nbgl_warning_t warning = {0};

// called when long press button on 3rd page is long-touched or when reject footer is touched
static void review_choice(bool confirm) {
    if (confirm) {
        // User approved transaction
        // Set state to APPROVED and return tx hash
        // Witnesses will be handled in separate APDUs
        G_context.state = STATE_APPROVED;

        // Initialize witness counters
        G_context.tx_info.current_witness = 0;
        // num_witnesses should be set during tx init (TODO: add to deserializer)

        // Send tx hash back to client
        io_send_response_pointer(G_context.tx_info.tx_hash, sizeof(G_context.tx_info.tx_hash), SW_OK);

        // Show status
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, ui_menu_main);
    } else {
        // User rejected
        G_context.state = STATE_NONE;

        // Free transaction buffer
        if (G_context.tx_info.raw_tx != NULL) {
            app_mem_free(G_context.tx_info.raw_tx);
            G_context.tx_info.raw_tx = NULL;
        }

        io_send_sw(SW_DENY);
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_REJECTED, ui_menu_main);
    }
}

// Public function to start the transaction review
// - Check if the app is in the right state for transaction review
// - Format the fee and output strings dynamically
// - Display the first screen of the transaction review
int ui_display_transaction(void) {
    if (G_context.req_type != REQUEST_CONFIRM_TRANSACTION || G_context.state != STATE_PARSED) {
        G_context.state = STATE_NONE;
        return io_send_sw(SW_BAD_STATE);
    }

    // Calculate number of pairs: (num_outputs * 2) + 1 for fee + (1 for TTL if included) + 1 for tx hash
    // Each output needs 2 pairs: address + amount
    g_num_pairs = (G_context.tx_info.transaction.num_outputs * 2) + 2;
    if (G_context.tx_info.transaction.includeTtl) {
        g_num_pairs++;  // Add 1 for TTL
    }

    // Allocate pairs array
    g_pairs = (nbgl_contentTagValue_t *) app_mem_alloc(g_num_pairs * sizeof(nbgl_contentTagValue_t));
    if (g_pairs == NULL) {
        return io_send_sw(SW_TX_PARSING_FAIL);
    }

    // Allocate array of string pointers for values
    g_pair_values = (char **) app_mem_alloc(g_num_pairs * sizeof(char *));
    if (g_pair_values == NULL) {
        return io_send_sw(SW_TX_PARSING_FAIL);
    }

    uint16_t pair_idx = 0;

    // Add fee first
    explicit_bzero(g_fee, sizeof(g_fee));
    str_formatAdaAmount(G_context.tx_info.transaction.fee, g_fee, sizeof(g_fee));
    g_pairs[pair_idx].item = "Fee";
    g_pairs[pair_idx].value = g_fee;
    pair_idx++;

    // Add TTL if included
    if (G_context.tx_info.transaction.includeTtl) {
        explicit_bzero(g_ttl, sizeof(g_ttl));
        ui_getUint64Screen(g_ttl, sizeof(g_ttl), G_context.tx_info.transaction.ttl);
        g_pairs[pair_idx].item = "TTL";
        g_pairs[pair_idx].value = g_ttl;
        pair_idx++;
    }

    // Add each output
    uint16_t output_num = 1;
    s_flist_node *node = G_context.tx_info.transaction.outputs;
    while (node != NULL) {
        tx_output_list_item_t *output_item = (tx_output_list_item_t *) node;

        // Allocate buffer for output label (e.g., "Output 1 Address")
        char *label = (char *) app_mem_alloc(32);
        if (label == NULL) {
            return io_send_sw(SW_TX_PARSING_FAIL);
        }
        snprintf(label, 32, "Output %d Address", output_num);
        g_pairs[pair_idx].item = label;

        // Allocate and format address using Bech32 (Cardano format)
        // MAX_HUMAN_ADDRESS_SIZE is defined in cardano.h as 150
        char *addr_str = (char *) app_mem_alloc(MAX_HUMAN_ADDRESS_SIZE);
        if (addr_str == NULL) {
            return io_send_sw(SW_TX_PARSING_FAIL);
        }

        size_t addr_len = 0;
        if (output_item->output_data.destination.type == DESTINATION_THIRD_PARTY) {
            // Use Cardano's humanReadableAddress function for proper Bech32 formatting
            addr_len = humanReadableAddress(
                output_item->output_data.destination.address.buffer,
                output_item->output_data.destination.address.size,
                addr_str,
                MAX_HUMAN_ADDRESS_SIZE
            );
        } else if (output_item->output_data.destination.type == DESTINATION_DEVICE_OWNED) {
            // Derive address from path
            uint8_t address_bytes[MAX_ADDRESS_SIZE];
            size_t address_size = deriveAddress(
                &output_item->output_data.destination.params,
                address_bytes,
                sizeof(address_bytes)
            );

            if (address_size > 0) {
                addr_len = humanReadableAddress(
                    address_bytes,
                    address_size,
                    addr_str,
                    MAX_HUMAN_ADDRESS_SIZE
                );
            }
        }

        if (addr_len == 0) {
            return io_send_sw(SW_DISPLAY_ADDRESS_FAIL);
        }

        g_pairs[pair_idx].value = addr_str;
        g_pair_values[pair_idx] = addr_str;
        pair_idx++;

        // Allocate buffer for amount label (e.g., "Output 1 Amount")
        label = (char *) app_mem_alloc(32);
        if (label == NULL) {
            return io_send_sw(SW_TX_PARSING_FAIL);
        }
        snprintf(label, 32, "Output %d Amount", output_num);
        g_pairs[pair_idx].item = label;

        // Allocate and format amount
        char *amount_str = (char *) app_mem_alloc(40);
        if (amount_str == NULL) {
            return io_send_sw(SW_TX_PARSING_FAIL);
        }
        char amount_formatted[30] = {0};
        if (!format_fpu64(amount_formatted,
                          sizeof(amount_formatted),
                          output_item->output_data.adaAmount,
                          EXPONENT_SMALLEST_UNIT)) {
            return io_send_sw(SW_DISPLAY_AMOUNT_FAIL);
        }
        snprintf(amount_str, 40, "BOL %.*s", sizeof(amount_formatted), amount_formatted);
        g_pairs[pair_idx].value = amount_str;
        g_pair_values[pair_idx] = amount_str;
        pair_idx++;

        output_num++;
        node = node->next;
    }

    // Add transaction hash as the last item
    g_pairs[pair_idx].item = "Transaction hash";
    char *tx_hash_str = (char *) app_mem_alloc(65);  // 32 bytes = 64 hex chars + null terminator
    if (tx_hash_str == NULL) {
        return io_send_sw(SW_TX_PARSING_FAIL);
    }
    ui_getHexBufferScreen(tx_hash_str, 65, G_context.tx_info.tx_hash, sizeof(G_context.tx_info.tx_hash));
    g_pairs[pair_idx].value = tx_hash_str;
    g_pair_values[pair_idx] = tx_hash_str;
    pair_idx++;

    // Setup list
    pairList.nbMaxLinesForValue = 0;
    pairList.nbPairs = g_num_pairs;
    pairList.pairs = g_pairs;

    // Check if we have warnings to display
    const nbgl_warning_t* warningPtr = NULL;
    if (!tx_warning_list_empty((tx_warning_list_item_t *)G_context.tx_info.warning_list)) {
        PRINTF("Warnings detected, preparing warning display\n");

        // Concatenate all warning messages
        explicit_bzero(g_warning_msg, sizeof(g_warning_msg));
        size_t offset = 0;
        tx_warning_list_item_t *warning_node = (tx_warning_list_item_t *)G_context.tx_info.warning_list;
        while (warning_node != NULL && offset < sizeof(g_warning_msg) - 2) {
            const char *msg = tx_warning_get_message(warning_node->type);
            size_t msg_len = strlen(msg);
            if (offset + msg_len + 2 < sizeof(g_warning_msg)) {
                if (offset > 0) {
                    g_warning_msg[offset++] = '\n';
                }
                memmove(g_warning_msg + offset, msg, msg_len);
                offset += msg_len;
            }
            warning_node = (tx_warning_list_item_t *)warning_node->node.next;
        }

        // Setup warning structures
        explicit_bzero(&warningInfo, sizeof(warningInfo));
        warningInfo.icon = &WARNING_ICON;
        warningInfo.title = "Transaction Warning";
        warningInfo.description = g_warning_msg;

        explicit_bzero(&warningDetails, sizeof(warningDetails));
        warningDetails.title = "Transaction Warning";
        warningDetails.type = CENTERED_INFO_WARNING;
        warningDetails.centeredInfo.icon = &WARNING_ICON;
        warningDetails.centeredInfo.title = "Transaction Warning";
        warningDetails.centeredInfo.description = g_warning_msg;

        explicit_bzero(&warning, sizeof(warning));
        warning.introDetails = &warningDetails;
        warning.reviewDetails = &warningDetails;
        warning.info = &warningInfo;
        warning.introTopRightIcon = &WARNING_ICON;
        warning.reviewTopRightIcon = &WARNING_ICON;
        warningPtr = &warning;

        PRINTF("Warning display prepared\n");
    }

    // Start review flow (with or without warnings)
    if (warningPtr != NULL) {
        // Use advanced review with warnings
        nbgl_useCaseAdvancedReview(TYPE_TRANSACTION,
                                  &pairList,
                                  &ICON_APP_CARDANO,
                                  "Review transaction",
                                  NULL,
                                  "Sign transaction",
                                  NULL,
                                  warningPtr,
                                  review_choice);
    } else {
        // Use simple review without warnings
        nbgl_useCaseReview(TYPE_TRANSACTION,
                          &pairList,
                          &ICON_APP_CARDANO,
                          "Review transaction",
                          NULL,
#ifdef SCREEN_SIZE_WALLET
                          "Sign transaction",
#else
                          NULL,
#endif
                          review_choice);
    }
    return 0;
}
