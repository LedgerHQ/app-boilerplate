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
#include "transaction/deserialize.h"
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

/**
 * Cleanup dynamically allocated buffers for transaction display
 */
static void tx_buffer_cleanup(void) {
    // Cleanup all tracked allocations (g_fee, g_ttl, g_warning_msg, warning structures, and per-output strings)
    ui_cleanup_tracked_allocations();
    // Cleanup the pairs array
    ui_pairs_cleanup();
}

/**
 * Cleanup transaction data after UI is finished
 * Frees the parsed transaction lists and warnings
 * (raw_tx is freed earlier in handler_sign_tx after deserialization)
 */
void tx_data_cleanup(void) {
    // Note: raw_tx buffer is freed in handler_sign_tx after deserialization completes
    // Check and free just in case it wasn't freed (defensive programming)
    if (G_context.tx_info.raw_tx != NULL) {
        app_mem_free(G_context.tx_info.raw_tx);
        G_context.tx_info.raw_tx = NULL;
    }
    // Free parsed transaction lists
    transaction_cleanup(&G_context.tx_info.transaction);
    // Free all accumulated warnings
    tx_warning_list_cleanup((tx_warning_list_item_t **)&G_context.tx_info.warning_list);
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
        // num_witnesses is set during P1_TX_INIT (sign_tx.c:118)

        // Send tx hash back to client
        io_send_response_pointer(G_context.tx_info.tx_hash, sizeof(G_context.tx_info.tx_hash), SW_OK);

        // Check if there are witnesses to process
        if (G_context.tx_info.num_witnesses > 0) {
            // Witnesses coming - show spinner while waiting for witness APDUs
            // Don't cleanup yet - witnesses still need the parsed transaction structures
            nbgl_useCaseSpinner("Processing");
        } else {
            // No witnesses - transaction is complete, cleanup and show status
            tx_data_cleanup();
            nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, ui_menu_main);
        }
    } else {
        // User rejected
        G_context.state = STATE_NONE;

        // Cleanup transaction data
        tx_data_cleanup();

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

    // Allocate display buffers using ui_mem_alloc for automatic tracking
    g_fee = (char *) ui_mem_alloc(MAX_ADA_AMOUNT_STRING_SIZE);
    if (g_fee == NULL) {
        tx_buffer_cleanup();
        return io_send_sw(SW_INSUFFICIENT_MEMORY);
    }
    g_ttl = (char *) ui_mem_alloc(MAX_ADA_AMOUNT_STRING_SIZE);
    if (g_ttl == NULL) {
        tx_buffer_cleanup();
        return io_send_sw(SW_INSUFFICIENT_MEMORY);
    }
    g_warning_msg = (char *) ui_mem_alloc(MAX_WARNING_MESSAGE_SIZE);
    if (g_warning_msg == NULL) {
        tx_buffer_cleanup();
        return io_send_sw(SW_INSUFFICIENT_MEMORY);
    }

    // Calculate number of pairs: (num_outputs * 2) + 1 for fee + (1 for TTL if included) + 1 for tx hash
    // Each output needs 2 pairs: address + amount
    uint16_t num_pairs = (G_context.tx_info.transaction.num_outputs * 2) + 2;
    if (G_context.tx_info.transaction.includeTtl) {
        num_pairs++;  // Add 1 for TTL
    }

    // Initialize common pairs structure
    if (!ui_pairs_init(num_pairs)) {
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

    // Add each output (1-indexed for display)
    uint16_t output_num = 1;
    s_flist_node *node = G_context.tx_info.transaction.outputs;
    while (node != NULL) {
        tx_output_list_item_t *output_item = (tx_output_list_item_t *) node;

        // Add output number pair (e.g., "Output" | "1")
        g_pairs[pair_idx].item = "Output";
        char *output_num_str = (char *) ui_mem_alloc(MAX_UINT64_STRING_SIZE);
        if (output_num_str == NULL) {
            tx_buffer_cleanup();
            return io_send_sw(SW_INSUFFICIENT_MEMORY);
        }
        snprintf(output_num_str, MAX_UINT64_STRING_SIZE, "%d", output_num);
        g_pairs[pair_idx].value = output_num_str;
        pair_idx++;

        // Add address pair (e.g., "Address" | "<address>")
        g_pairs[pair_idx].item = "Address";

        // Allocate and format address using Bech32 (Cardano format)
        // MAX_HUMAN_ADDRESS_SIZE is defined in cardano.h as 150
        char *addr_str = (char *) ui_mem_alloc(MAX_HUMAN_ADDRESS_SIZE);
        if (addr_str == NULL) {
            tx_buffer_cleanup();
            return io_send_sw(SW_INSUFFICIENT_MEMORY);
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

        // Add amount pair with label "Amount"
        g_pairs[pair_idx].item = "Amount";

        // Allocate and format amount with currency
        char *amount_str = (char *) ui_mem_alloc(MAX_AMOUNT_DISPLAY_SIZE);
        if (amount_str == NULL) {
            tx_buffer_cleanup();
            return io_send_sw(SW_INSUFFICIENT_MEMORY);
        }
        char amount_formatted[30] = {0};
        if (!format_fpu64(amount_formatted,
                          sizeof(amount_formatted),
                          output_item->output_data.adaAmount,
                          EXPONENT_SMALLEST_UNIT)) {
            return io_send_sw(SW_DISPLAY_AMOUNT_FAIL);
        }
        snprintf(amount_str, MAX_AMOUNT_DISPLAY_SIZE, "BOL %.*s", sizeof(amount_formatted), amount_formatted);
        g_pairs[pair_idx].value = amount_str;
        pair_idx++;

        output_num++;
        node = node->next;
    }

    // Add transaction hash as the last item
    g_pairs[pair_idx].item = "Transaction hash";
    char *tx_hash_str = (char *) ui_mem_alloc(MAX_TX_HASH_DISPLAY_SIZE);
    if (tx_hash_str == NULL) {
        tx_buffer_cleanup();
        return io_send_sw(SW_INSUFFICIENT_MEMORY);
    }
    ui_getHexBufferScreen(tx_hash_str, MAX_TX_HASH_DISPLAY_SIZE, G_context.tx_info.tx_hash, sizeof(G_context.tx_info.tx_hash));
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

        // Allocate and setup warning structures using ui_mem_alloc for automatic tracking
        g_warningInfo = (nbgl_contentCenter_t *) ui_mem_alloc(sizeof(nbgl_contentCenter_t));
        if (g_warningInfo == NULL) {
            tx_buffer_cleanup();
            return io_send_sw(SW_INSUFFICIENT_MEMORY);
        }
        g_warningDetails = (nbgl_warningDetails_t *) ui_mem_alloc(sizeof(nbgl_warningDetails_t));
        if (g_warningDetails == NULL) {
            tx_buffer_cleanup();
            return io_send_sw(SW_INSUFFICIENT_MEMORY);
        }
        g_warning = (nbgl_warning_t *) ui_mem_alloc(sizeof(nbgl_warning_t));
        if (g_warning == NULL) {
            tx_buffer_cleanup();
            return io_send_sw(SW_INSUFFICIENT_MEMORY);
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
