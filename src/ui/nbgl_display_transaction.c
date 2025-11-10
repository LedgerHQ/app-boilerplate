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
#include "ui_utils.h"
#include "mem_utils.h"

// Dynamically allocated buffers for transaction display
static char *g_fee = NULL;
static char *g_ttl = NULL;
static char *g_warning_msg = NULL;
static nbgl_contentCenter_t *g_warningInfo = NULL;
static nbgl_warningDetails_t *g_warningDetails = NULL;
static nbgl_warning_t *g_warning = NULL;

// Number of pairs in the current transaction display
static uint16_t g_num_pairs = 0;

/**
 * Cleanup dynamically allocated buffers for transaction display
 */
static void tx_buffer_cleanup(void) {
    mem_buffer_cleanup((void **) &g_fee);
    mem_buffer_cleanup((void **) &g_ttl);
    mem_buffer_cleanup((void **) &g_warning_msg);
    mem_buffer_cleanup((void **) &g_warningInfo);
    mem_buffer_cleanup((void **) &g_warningDetails);
    mem_buffer_cleanup((void **) &g_warning);
    ui_pairs_cleanup();
}

// called when long press button on 3rd page is long-touched or when reject footer is touched
static void review_choice(bool confirm) {
    // Cleanup display buffers
    tx_buffer_cleanup();

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

    // Allocate display buffers
    if (!mem_buffer_allocate((void **) &g_fee, MAX_ADA_AMOUNT_STRING_SIZE)) {
        tx_buffer_cleanup();
        return io_send_sw(SW_TX_PARSING_FAIL);
    }
    if (!mem_buffer_allocate((void **) &g_ttl, MAX_ADA_AMOUNT_STRING_SIZE)) {
        tx_buffer_cleanup();
        return io_send_sw(SW_TX_PARSING_FAIL);
    }
    if (!mem_buffer_allocate((void **) &g_warning_msg, MAX_WARNING_MESSAGE_SIZE)) {
        tx_buffer_cleanup();
        return io_send_sw(SW_TX_PARSING_FAIL);
    }

    // Calculate number of pairs: (num_outputs * 2) + 1 for fee + (1 for TTL if included) + 1 for tx hash
    // Each output needs 2 pairs: address + amount
    g_num_pairs = (G_context.tx_info.transaction.num_outputs * 2) + 2;
    if (G_context.tx_info.transaction.includeTtl) {
        g_num_pairs++;  // Add 1 for TTL
    }

    // Initialize common pairs structure
    if (!ui_pairs_init(g_num_pairs)) {
        tx_buffer_cleanup();
        return io_send_sw(SW_TX_PARSING_FAIL);
    }

    uint16_t pair_idx = 0;

    // Add fee first
    str_formatAdaAmount(G_context.tx_info.transaction.fee, g_fee, MAX_ADA_AMOUNT_STRING_SIZE);
    g_pairs[pair_idx].item = "Fee";
    g_pairs[pair_idx].value = g_fee;
    pair_idx++;

    // Add TTL if included
    if (G_context.tx_info.transaction.includeTtl) {
        ui_getUint64Screen(g_ttl, MAX_ADA_AMOUNT_STRING_SIZE, G_context.tx_info.transaction.ttl);
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
    pair_idx++;

    // Check if we have warnings to display
    const nbgl_warning_t* warningPtr = NULL;
    if (!tx_warning_list_empty((tx_warning_list_item_t *)G_context.tx_info.warning_list)) {
        PRINTF("Warnings detected, preparing warning display\n");

        // Concatenate all warning messages
        size_t offset = 0;
        tx_warning_list_item_t *warning_node = (tx_warning_list_item_t *)G_context.tx_info.warning_list;
        while (warning_node != NULL && offset < MAX_WARNING_MESSAGE_SIZE - 2) {
            const char *msg = tx_warning_get_message(warning_node->type);
            size_t msg_len = strlen(msg);
            if (offset + msg_len + 2 < MAX_WARNING_MESSAGE_SIZE) {
                if (offset > 0) {
                    g_warning_msg[offset++] = '\n';
                }
                memmove(g_warning_msg + offset, msg, msg_len);
                offset += msg_len;
            }
            warning_node = (tx_warning_list_item_t *)warning_node->node.next;
        }

        // Allocate and setup warning structures
        if (!mem_buffer_allocate((void **) &g_warningInfo, sizeof(nbgl_contentCenter_t))) {
            tx_buffer_cleanup();
            return io_send_sw(SW_TX_PARSING_FAIL);
        }
        if (!mem_buffer_allocate((void **) &g_warningDetails, sizeof(nbgl_warningDetails_t))) {
            tx_buffer_cleanup();
            return io_send_sw(SW_TX_PARSING_FAIL);
        }
        if (!mem_buffer_allocate((void **) &g_warning, sizeof(nbgl_warning_t))) {
            tx_buffer_cleanup();
            return io_send_sw(SW_TX_PARSING_FAIL);
        }

        // Setup warning content
        g_warningInfo->icon = &WARNING_ICON;
        g_warningInfo->title = "Transaction Warning";
        g_warningInfo->description = g_warning_msg;

        g_warningDetails->title = "Transaction Warning";
        g_warningDetails->type = CENTERED_INFO_WARNING;
        g_warningDetails->centeredInfo.icon = &WARNING_ICON;
        g_warningDetails->centeredInfo.title = "Transaction Warning";
        g_warningDetails->centeredInfo.description = g_warning_msg;

        g_warning->introDetails = g_warningDetails;
        g_warning->reviewDetails = g_warningDetails;
        g_warning->info = g_warningInfo;
        g_warning->introTopRightIcon = &WARNING_ICON;
        g_warning->reviewTopRightIcon = &WARNING_ICON;
        warningPtr = g_warning;

        PRINTF("Warning display prepared\n");
    }

    // Start review flow (with or without warnings)
    if (warningPtr != NULL) {
        // Use advanced review with warnings
        nbgl_useCaseAdvancedReview(TYPE_TRANSACTION,
                                  g_pairsList,
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
                          g_pairsList,
                          &ICON_APP_CARDANO,
                          "Review transaction",
                          NULL,
#ifdef SCREEN_SIZE_WALLET // TODO what is this?
                          "Sign transaction",
#else
                          NULL,
#endif
                          review_choice);
    }
    return 0;
}
