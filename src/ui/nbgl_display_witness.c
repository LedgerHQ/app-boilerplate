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
#include "nbgl_use_case.h"
#include "io.h"
#include "addressUtils/bip44.h"
#include "format.h"

#include "display.h"
#include "constants.h"
#include "globals.h"
#include "sw.h"
#include "securityPolicy.h"
#include "nbgl_screens.h"
#include "menu.h"
#include "mem_utils.h"
#include "mem.h"
#include "transaction/deserialize.h"

static char *witnessPathStr = NULL;

/**
 * Cleanup dynamically allocated buffers for witness display
 */
static void witness_buffers_cleanup(void) {
    mem_buffer_cleanup((void **) &witnessPathStr);
}

static void witness_review_choice(bool confirm) {
    witness_buffers_cleanup();

    if (!confirm) {
        // User rejected the witness - abort further witness processing
        G_context.state.tx_state = TX_STATE_NONE;
        // Cleanup transaction data since we're aborting
        tx_data_cleanup();
        io_send_sw(SW_DENY);
        nbgl_useCaseStatus("Witness\ndenied", true, ui_menu_main);
    } else {
        // Witness confirmed - send signature back
        io_send_response_pointer(G_context.tx_info.witness_signature,
                                ED25519_SIGNATURE_LENGTH,
                                SW_OK);

        // Increment witness counter for next iteration
        G_context.tx_info.current_witness++;

        // Check if there are more witnesses to process
        if (G_context.tx_info.current_witness < G_context.tx_info.num_witnesses) {
            // More witnesses to come - show spinner while waiting for next witness
            // Don't cleanup yet - still need parsed transaction for remaining witnesses
            nbgl_useCaseSpinner("Processing");
        } else {
            // All witnesses processed - cleanup transaction data and show completion
            tx_data_cleanup();
            nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, ui_menu_main);
        }
    }
}

int ui_display_witness(const bip44_path_t* witnessPath, security_policy_t securityPolicy) {
    TRACE("=== ui_display_witness START ===");
    TRACE("securityPolicy: %d", securityPolicy);

    if (G_context.state.tx_state != TX_STATE_APPROVED || G_context.req_type != REQUEST_SIGN_TRANSACTION) {
        TRACE("Bad state detected - returning error");
        G_context.state.tx_state = TX_STATE_NONE;
        return io_send_sw(SW_BAD_STATE);
    }

    // Allocate display buffers
    if (!mem_buffer_allocate((void **) &witnessPathStr, BIP44_PATH_STRING_SIZE_MAX + 1)) {
        witness_buffers_cleanup();
        return io_send_sw(SW_DISPLAY_BIP32_PATH_FAIL);
    }

    // Set warning if needed
    bool isUnusual = false;
    switch (securityPolicy) {
        case POLICY_PROMPT_WARN_UNUSUAL:
            isUnusual = true;
            break;

        case POLICY_SHOW_BEFORE_RESPONSE:
            // No warning, just display the witness path
            isUnusual = false;
            break;

        case POLICY_PROMPT_BEFORE_RESPONSE:
            // No warning, just prompt for confirmation
            isUnusual = false;
            break;

        default:
            // Catch any truly unknown or unexpected policy values
            ASSERT(false);
            witness_buffers_cleanup();
            return 0;
    }

    TRACE("isUnusual: %d", isUnusual);

    // Format the witness path as a string
    ui_getPathScreen(witnessPathStr, BIP44_PATH_STRING_SIZE_MAX + 1, witnessPath);

    if (isUnusual) {
        // A mild warning about unusual path
        // No immediate threat, just to be aware that the witness key is unusual
        nbgl_useCaseChoice(
            &WARNING_ICON,
            "Sign with UNUSUAL key",
            witnessPathStr,
            "Confirm",
            "Reject",
            witness_review_choice
        );
    } else {
        // Normal path display
        nbgl_useCaseChoice(
            &ICON_APP_CARDANO,
            "Witness",
            witnessPathStr,
            "Confirm",
            "Reject",
            witness_review_choice
        );
    }

    return 0;
}
