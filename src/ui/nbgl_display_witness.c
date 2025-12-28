/*****************************************************************************
 *   Ledger App Cardano.
 *   (c) 2025 Vacuumlabs
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
#include "globals.h"
#include "utils/utils.h"
#include "utils/cardano_os_utils.h"
#include "cardano_swo.h"
#include "securityPolicy.h"
#include "nbgl_screens.h"
#include "menu.h"
#include "memory/mem_utils.h"
#include "memory/mem.h"
#include "transaction/tx_parse.h"
#include "ui_utils.h"
#include "handler/sign_tx.h"

/**
 * Callback when user confirms or rejects witness display
 * Cleans up allocated UI resources and processes the witness accordingly
 */
static void witness_review_choice(bool confirm) {
    ui_cleanup_tracked_allocations();

    if (!confirm) {
        // User rejected the witness - abort further witness processing
        tx_context_cleanup();

        send_error_and_reset(SWO_CONDITIONS_NOT_SATISFIED);
        nbgl_useCaseStatus("Witness\ndenied", true, ui_menu_main);
    } else {
        finalize_witness();
    }
}

int ui_display_witness(const bip44_path_t* witnessPath,
                       security_policy_t securityPolicy,
                       warning_bits_t warnings) {
    TRACE("=== ui_display_witness START ===");
    TRACE("securityPolicy: %d", securityPolicy);

    if (G_context.state.tx_state != TX_STATE_APPROVED || G_context.req_type != REQUEST_SIGN_TRANSACTION) {
        TRACE("Bad state detected - returning error");
        tx_context_cleanup();
        return send_error_and_reset(SWO_BAD_STATE);
    }

    // Allocate display buffer for witness path using UI tracking system
    // This ensures automatic cleanup when the user responds or on error
    const size_t witnessPathStrSize = MAX_BIP44_PATH_STRING_LENGTH + 1;
    char *witnessPathStr = (char *) ui_mem_alloc(witnessPathStrSize);
    if (witnessPathStr == NULL) {
        TRACE("Failed to allocate witness path string");
        ui_cleanup_tracked_allocations();
        tx_context_cleanup();
        return send_error_and_reset(SWO_DISPLAY_BIP32_PATH_FAIL);
    }

    bool isUnusual = warning_bits_has(warnings, WARNING_BIT_UNUSUAL_KEY_DERIVATION_PATH);

    if (securityPolicy != POLICY_SHOW) {
        ASSERT(false);
        ui_cleanup_tracked_allocations();
        return send_error_and_reset(SWO_BAD_STATE);
    }

    TRACE("isUnusual: %d", isUnusual);

    // Format the witness path as a string
    explicit_bzero(witnessPathStr, witnessPathStrSize);
    bool formatted = format_bip44_path(witnessPath, witnessPathStr, witnessPathStrSize);
    ASSERT(formatted);
    ASSERT(strlen(witnessPathStr) + 1 < witnessPathStrSize);

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
