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

#include "display.h"
#include "ui/ui_constants.h"
#include "globals.h"
#include "utils/utils.h"
#include "app_context.h"
#include "cardano_swo.h"
#include "opcert_types.h"
#include "menu.h"
#include "securityPolicy.h"
#include "nbgl_screens.h"
#include "sign_opcert.h"
#include "memory/mem_utils.h"
#include "ui_utils.h"

// Dynamic buffers for reduced stack pressure during signing
static char *poolColdKeyPathStr = NULL;
static char *poolKeyHashStr = NULL;
static char *kesKeyStr = NULL;
static char *kesPeriodStr = NULL;
static char *issueCounterStr = NULL;
static nbgl_warning_t *g_warning = NULL;

// Centered info for the main warning screen.

/**
 * Cleanup dynamically allocated buffers
 */
static void opcert_buffer_cleanup(void) {
    // Cleanup all tracked allocations (all string buffers and warning structure)
    ui_cleanup_tracked_allocations();
    // Cleanup the pairs array
    ui_pairs_cleanup();
}

// called when long press button on 3rd page is long-touched or when reject footer is touched
static void opcert_review_choice(bool confirm) {
    opcert_buffer_cleanup();

    finalize_sign_opcert(confirm);

    if (confirm) {
        TRACE("User confirmed - showing signed status");
        TRACE("Calling nbgl_useCaseReviewStatus(STATUS_TYPE_OPERATION_SIGNED, ui_menu_main)");
        nbgl_useCaseReviewStatus(STATUS_TYPE_OPERATION_SIGNED, ui_menu_main);
    } else {
        TRACE("User rejected - showing rejected status");
        TRACE("Calling nbgl_useCaseReviewStatus(STATUS_TYPE_OPERATION_REJECTED, ui_menu_main)");
        nbgl_useCaseReviewStatus(STATUS_TYPE_OPERATION_REJECTED, ui_menu_main);
    }
}

int ui_display_opcert(security_policy_t securityPolicy, warning_bits_t warnings) {
    TRACE("=== ui_display_opcert START ===");
    TRACE("securityPolicy: %d", securityPolicy);

    if (G_context.req_type != REQUEST_SIGN_OPCERT || G_context.state.opcert_state != OPCERT_STATE_PARSED) {
        TRACE("Bad state detected - returning error");
        return send_swo_and_reset(SWO_BAD_STATE);
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
            return 0;

        default:
            ASSERT(false);
            return send_swo_and_reset(SWO_BAD_STATE);
    }

    const parsed_opcert_t* opcert = &G_context.opcert_info.opcert;

    // Allocate and fill pool cold key path
    poolColdKeyPathStr = (char *) ui_mem_alloc(MAX_BIP44_PATH_STRING_LENGTH + 2);
    if (poolColdKeyPathStr == NULL) {
        TRACE("Failed to allocate poolColdKeyPathStr");
        opcert_buffer_cleanup();
        return send_swo_and_reset(SWO_INSUFFICIENT_MEMORY);
    }
    bool poolPathFormatted = format_bip44_path(&opcert->poolColdKeyPath,
                                               poolColdKeyPathStr,
                                               MAX_BIP44_PATH_STRING_LENGTH + 2);
    ASSERT(poolPathFormatted);
    LEDGER_ASSERT(strlen(poolColdKeyPathStr) <= MAX_BIP44_PATH_STRING_LENGTH, "Pool cold key path ui string buffer too short");

    // Allocate and fill pool ID (key hash)
    poolKeyHashStr = (char *) ui_mem_alloc(MAX_BECH32_STRING_LENGTH + 2);
    if (poolKeyHashStr == NULL) {
        TRACE("Failed to allocate poolKeyHashStr");
        opcert_buffer_cleanup();
        return send_swo_and_reset(SWO_INSUFFICIENT_MEMORY);
    }
    uint8_t poolKeyHash[POOL_KEY_HASH_LENGTH] = {0};
    bip44_pathToKeyHash(&opcert->poolColdKeyPath, poolKeyHash, SIZEOF(poolKeyHash));
    bool pool_key_formatted = format_bech32("pool",
                                            poolKeyHash,
                                            SIZEOF(poolKeyHash),
                                            poolKeyHashStr,
                                            MAX_BECH32_STRING_LENGTH + 2);
    ASSERT(pool_key_formatted);
    LEDGER_ASSERT(strlen(poolKeyHashStr) <= MAX_BECH32_STRING_LENGTH, "Pool key hash ui string buffer too short");

    // Allocate and fill KES public key
    kesKeyStr = (char *) ui_mem_alloc(MAX_BECH32_STRING_LENGTH + 2);
    if (kesKeyStr == NULL) {
        TRACE("Failed to allocate kesKeyStr");
        opcert_buffer_cleanup();
        return send_swo_and_reset(SWO_INSUFFICIENT_MEMORY);
    }
    bool kes_key_formatted = format_bech32("kes_vk",
                                           opcert->kesPublicKey,
                                           KES_PUBLIC_KEY_LENGTH,
                                           kesKeyStr,
                                           MAX_BECH32_STRING_LENGTH + 2);
    ASSERT(kes_key_formatted);
    LEDGER_ASSERT(strlen(kesKeyStr) <= MAX_BECH32_STRING_LENGTH, "KES key ui string buffer too short");

    // Allocate and fill KES period
    kesPeriodStr = (char *) ui_mem_alloc(MAX_UINT64_STRING_LENGTH + 2);
    if (kesPeriodStr == NULL) {
        TRACE("Failed to allocate kesPeriodStr");
        opcert_buffer_cleanup();
        return send_swo_and_reset(SWO_INSUFFICIENT_MEMORY);
    }
    bool format_ok = format_u64(kesPeriodStr, MAX_UINT64_STRING_LENGTH + 2, opcert->kesPeriod);
    LEDGER_ASSERT(format_ok, "Failed to format KES period");
    LEDGER_ASSERT(strlen(kesPeriodStr) <= MAX_UINT64_STRING_LENGTH, "KES period ui string buffer too short");

    // Allocate and fill issue counter
    issueCounterStr = (char *) ui_mem_alloc(MAX_UINT64_STRING_LENGTH + 2);
    if (issueCounterStr == NULL) {
        TRACE("Failed to allocate issueCounterStr");
        opcert_buffer_cleanup();
        return send_swo_and_reset(SWO_INSUFFICIENT_MEMORY);
    }
    format_ok = format_u64(issueCounterStr, MAX_UINT64_STRING_LENGTH + 2, opcert->issueCounter);
    LEDGER_ASSERT(format_ok, "Failed to format issue counter");
    LEDGER_ASSERT(strlen(issueCounterStr) <= MAX_UINT64_STRING_LENGTH, "Issue counter ui string buffer too short");

    // Setup data to display
    if (!ui_pairs_init(5)) {
        TRACE("Failed to initialize pairs");
        opcert_buffer_cleanup();
        return send_swo_and_reset(SWO_INSUFFICIENT_MEMORY);
    }
    g_pairs[0].item = "Pool cold key path";
    g_pairs[0].value = poolColdKeyPathStr;
    g_pairs[1].item = "Pool ID";
    g_pairs[1].value = poolKeyHashStr;
    g_pairs[2].item = "KES public key";
    g_pairs[2].value = kesKeyStr;
    g_pairs[3].item = "KES period";
    g_pairs[3].value = kesPeriodStr;
    g_pairs[4].item = "Issue counter";
    g_pairs[4].value = issueCounterStr;

    // set warning if needed
    const nbgl_warning_t* warningPtr = NULL;
    TRACE("Security policy received: %d", securityPolicy);
    const warning_definition_t* warning_defs[WARNING_BIT_COUNT];
    size_t warning_count =
        warning_bits_to_definitions(warnings, warning_defs, WARNING_BIT_COUNT);
    const warning_definition_t* def = NULL;
    const char* warning_title = NULL;
    const char* warning_description = NULL;
    bool warning_available = false;

    if (warning_bits_has(warnings, WARNING_BIT_UNUSUAL_KEY_DERIVATION_PATH)) {
        warning_title = (const char*) PIC("Unusual pool cold key path");
        warning_description =
            (const char*) PIC("Pool cold key derivation path is outside the standard account/index range.");
        warning_available = true;
    } else if (warning_count > 0) {
        def = (const warning_definition_t*) PIC(warning_defs[0]);
        if (def == NULL) {
            TRACE("Warning definition missing");
        } else {
            TRACE("Setting up warning for bit %d", def->bit);
            warning_title = (const char*) PIC(def->title);
            warning_description = (const char*) PIC(def->description);
            if (warning_title == NULL || warning_description == NULL) {
                TRACE("Warning title or description missing");
            } else {
                warning_available = true;
            }
        }
    }

    if (warning_available) {
        nbgl_contentCenter_t* info = (nbgl_contentCenter_t *) ui_mem_alloc(sizeof(nbgl_contentCenter_t));
        if (info == NULL) {
            TRACE("Failed to allocate warning info");
            opcert_buffer_cleanup();
            return send_swo_and_reset(SWO_INSUFFICIENT_MEMORY);
        }
        nbgl_warningDetails_t* details =
            (nbgl_warningDetails_t *) ui_mem_alloc(sizeof(nbgl_warningDetails_t));
        if (details == NULL) {
            TRACE("Failed to allocate warning details");
            opcert_buffer_cleanup();
            return send_swo_and_reset(SWO_INSUFFICIENT_MEMORY);
        }
        g_warning = (nbgl_warning_t *) ui_mem_alloc(sizeof(nbgl_warning_t));
        if (g_warning == NULL) {
            TRACE("Failed to allocate warning structure");
            opcert_buffer_cleanup();
            return send_swo_and_reset(SWO_INSUFFICIENT_MEMORY);
        }

        info->icon = &WARNING_ICON;
        info->title = warning_title;
        info->description = warning_description;

        details->title = warning_title;
        details->type = CENTERED_INFO_WARNING;
        details->centeredInfo.icon = &WARNING_ICON;
        details->centeredInfo.title = warning_title;
        details->centeredInfo.description = warning_description;

        g_warning->introDetails = details;
        g_warning->reviewDetails = details;
        g_warning->info = info;
        g_warning->introTopRightIcon = &WARNING_ICON;
        g_warning->reviewTopRightIcon = &WARNING_ICON;

        warningPtr = g_warning;
    }

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

    return 0;
}
