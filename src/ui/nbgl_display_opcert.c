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
#include "addressUtils/bech32.h"
#include "format.h"

#include "ui/ui_constants.h"
#include "ui/ui_icons.h"
#include "ui/ui_formatters.h"
#include "globals.h"
#include "utils/utils.h"
#include "app_context.h"
#include "cardano_swo.h"
#include "opcert_types.h"
#include "menu.h"
#include "securityPolicy.h"
#include "sign_opcert.h"
#include "memory/mem.h"
#include "memory/mem_utils.h"
#include "ui_utils.h"
#include "ui_warnings.h"
#include "ui_display_opcert.h"

/**
 * Cleanup dynamically allocated buffers and UI pairs
 */
static void opcert_buffer_cleanup(void) {
    // Cleanup all tracked allocations (warning structure and shrunk string buffers from pairs)
    ui_cleanup_tracked_allocations();
    // Cleanup the pairs array
    ui_pairs_cleanup();
}

/**
 * Format all operational certificate fields and add to UI pairs.
 *
 * @param opcert Parsed operational certificate data
 * @return UI_STATUS_SUCCESS on success, UI_STATUS_OUT_OF_MEMORY on allocation failure
 */
static ui_status_t format_opcert_fields(const parsed_opcert_t* opcert) {
    ui_reset_error_status();

    if (!ui_pairs_init(5)) {
        TRACE("Failed to initialize pairs");
        return UI_STATUS_OUT_OF_MEMORY;
    }

    // Format and add all opcert fields using unified macros
    UI_ADD_FORMAT1(UI_STATIC_LABEL("Pool cold key path"),
                   MAX_BIP44_PATH_STRING_LENGTH,
                   format_bip44_path,
                   &opcert->poolColdKeyPath);

    // Pool ID requires computing key hash from path
    uint8_t poolKeyHash[POOL_KEY_HASH_LENGTH] = {0};
    bip44_pathToKeyHash(&opcert->poolColdKeyPath, poolKeyHash, SIZEOF(poolKeyHash));
    UI_ADD_FORMAT3(UI_STATIC_LABEL("Pool ID"),
                   MAX_BECH32_STRING_LENGTH,
                   format_bech32,
                   "pool", poolKeyHash, POOL_KEY_HASH_LENGTH);

    UI_ADD_FORMAT3(UI_STATIC_LABEL("KES public key"),
                   MAX_BECH32_STRING_LENGTH,
                   format_bech32,
                   "kes_vk", opcert->kesPublicKey, KES_PUBLIC_KEY_LENGTH);

    UI_ADD_FORMAT1(UI_STATIC_LABEL("KES period"),
                   MAX_UINT64_STRING_LENGTH,
                   format_uint64,
                   opcert->kesPeriod);

    UI_ADD_FORMAT1(UI_STATIC_LABEL("Issue counter"),
                   MAX_UINT64_STRING_LENGTH,
                   format_uint64,
                   opcert->issueCounter);

    return ui_get_error_status();
}

// called when long press button on 3rd page is long-touched or when reject footer is touched
static void opcert_review_choice(bool confirm) {
    // CLEANUP
    opcert_buffer_cleanup();

    // FINALIZE
    finalize_sign_opcert(confirm);

    // SHOW STATUS
    if (confirm) {
        TRACE("Calling nbgl_useCaseReviewStatus(STATUS_TYPE_OPERATION_SIGNED, ui_menu_main)");
        nbgl_useCaseReviewStatus(STATUS_TYPE_OPERATION_SIGNED, ui_menu_main);
    } else {
        TRACE("Calling nbgl_useCaseReviewStatus(STATUS_TYPE_OPERATION_REJECTED, ui_menu_main)");
        nbgl_useCaseReviewStatus(STATUS_TYPE_OPERATION_REJECTED, ui_menu_main);
    }
}

void ui_display_opcert(security_policy_t securityPolicy, warning_bits_t warnings) {
    TRACE("=== ui_display_opcert START ===");
    TRACE("securityPolicy: %d", securityPolicy);

    if (G_context.req_type != REQUEST_SIGN_OPCERT || G_context.state.opcert_state != OPCERT_STATE_PARSED) {
        TRACE("Bad state detected - returning error");
        send_swo_and_reset(SWO_BAD_STATE);
    }

    // Handle security policy
    switch (securityPolicy) {
        case POLICY_SHOW:
            // Continue to show UI
            break;

        case POLICY_HIDE:
            // Silent approval - finalize without showing UI
            TRACE("POLICY_HIDE: silently approving opcert");
            finalize_sign_opcert(true);
            return;

        default:
            ASSERT(false);
            send_swo_and_reset(SWO_BAD_STATE);
    }

    // Format all opcert fields and check for errors
    ui_status_t format_status = format_opcert_fields(&G_context.opcert_info.opcert);
    switch (format_status) {
        case UI_STATUS_SUCCESS:
            break;
        case UI_STATUS_OUT_OF_MEMORY:
            send_swo_and_reset(SWO_INSUFFICIENT_MEMORY);
        case UI_STATUS_UNINITIALIZED:
        default:
            ASSERT(false);
            send_swo_and_reset(SWO_BAD_STATE);
    }

    // Build warnings if needed
    TRACE("Security policy received: %d", securityPolicy);
    ui_status_t warning_status = ui_build_warnings(warnings);
    switch (warning_status) {
        case UI_STATUS_SUCCESS:
            break;
        case UI_STATUS_OUT_OF_MEMORY:
            send_swo_and_reset(SWO_INSUFFICIENT_MEMORY);
        case UI_STATUS_UNINITIALIZED:
        default:
            ASSERT(false);
            send_swo_and_reset(SWO_BAD_STATE);
    }
    const nbgl_warning_t* warningPtr = ui_get_warnings();

    TRACE("Calling nbgl_useCaseAdvancedReview(TYPE_OPERATION)");
    nbgl_useCaseAdvancedReview(TYPE_OPERATION,
                        g_pairsList,
                        &ICON_APP_CARDANO,
                        "Sign operational\ncertificate",
                        NULL,
                        "Sign certificate",
                        NULL,
                        warningPtr,
                        opcert_review_choice
    );

    return;
}
