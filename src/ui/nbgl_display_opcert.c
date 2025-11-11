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
#include "bip44.h"
#include "format.h"

#include "display.h"
#include "constants.h"
#include "globals.h"
#include "sw.h"
#include "opcert_types.h"
#include "menu.h"
#include "securityPolicy.h"
#include "nbgl_screens.h"
#include "sign_opcert.h"
#include "mem_utils.h"
#include "ui_utils.h"

// Dynamic buffers for reduced stack pressure during signing
static char *poolColdKeyPathStr = NULL;
static char *poolKeyHashStr = NULL;
static char *kesKeyStr = NULL;
static char *kesPeriodStr = NULL;
static char *issueCounterStr = NULL;
static nbgl_warning_t *g_warning = NULL;

// Centered info for the main warning screen.
static const nbgl_contentCenter_t warningInfo = {
  .icon          = &WARNING_ICON,
  .title         = "Suspicious derivation path",
  .description   = "Pool cold key path seems unusual"
};

// Details page shown when the user taps the top-right icon.
static const nbgl_warningDetails_t warningDetails = {
  .title                   = "Suspicious derivation path",
  .type                    = CENTERED_INFO_WARNING,
  .centeredInfo.icon       = &WARNING_ICON,
  .centeredInfo.title      = "Suspicious derivation path",
  .centeredInfo.description= "Pool cold key path seems unusual"
};

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
static void review_choice(bool confirm) {
    opcert_buffer_cleanup();

    finalize_sign_opcert(confirm);

    if (confirm) {
        TRACE("User confirmed - showing signed status");
        nbgl_useCaseReviewStatus(STATUS_TYPE_OPERATION_SIGNED, ui_menu_main);
    } else {
        TRACE("User rejected - showing rejected status");
        nbgl_useCaseReviewStatus(STATUS_TYPE_OPERATION_REJECTED, ui_menu_main);
    }
}

int ui_display_opcert(security_policy_t securityPolicy) {
    TRACE("=== ui_display_opcert START ===");
    TRACE("securityPolicy: %d", securityPolicy);

    if (G_context.req_type != REQUEST_SIGN_OPCERT || G_context.state != STATE_PARSED) {
        TRACE("Bad state detected - returning error");
        G_context.state = STATE_NONE;
        return io_send_sw(SW_BAD_STATE);
    }

    const parsed_opcert_t* opcert = &G_context.opcert_info.opcert;

    // Allocate and fill pool cold key path
    poolColdKeyPathStr = (char *) ui_mem_alloc(BIP44_PATH_STRING_SIZE_MAX + 1);
    if (poolColdKeyPathStr == NULL) {
        TRACE("Failed to allocate poolColdKeyPathStr");
        opcert_buffer_cleanup();
        return io_send_sw(SW_INSUFFICIENT_MEMORY);
    }
    ui_getPathScreen(poolColdKeyPathStr, BIP44_PATH_STRING_SIZE_MAX + 1, &opcert->poolColdKeyPath);

    // Allocate and fill pool ID (key hash)
    poolKeyHashStr = (char *) ui_mem_alloc(BECH32_STRING_SIZE_MAX);
    if (poolKeyHashStr == NULL) {
        TRACE("Failed to allocate poolKeyHashStr");
        opcert_buffer_cleanup();
        return io_send_sw(SW_INSUFFICIENT_MEMORY);
    }
    uint8_t poolKeyHash[POOL_KEY_HASH_LENGTH] = {0};
    bip44_pathToKeyHash(&opcert->poolColdKeyPath, poolKeyHash, SIZEOF(poolKeyHash));
    ui_getBech32Screen(poolKeyHashStr,
                        BECH32_STRING_SIZE_MAX,
                        "pool",
                        poolKeyHash,
                        SIZEOF(poolKeyHash));

    // Allocate and fill KES public key
    kesKeyStr = (char *) ui_mem_alloc(BECH32_STRING_SIZE_MAX);
    if (kesKeyStr == NULL) {
        TRACE("Failed to allocate kesKeyStr");
        opcert_buffer_cleanup();
        return io_send_sw(SW_INSUFFICIENT_MEMORY);
    }
    ui_getBech32Screen(kesKeyStr,
                        BECH32_STRING_SIZE_MAX,
                        "kes_vk",
                        opcert->kesPublicKey,
                        KES_PUBLIC_KEY_LENGTH);

    // Allocate and fill KES period
    kesPeriodStr = (char *) ui_mem_alloc(MAX_UINT64_STRING_SIZE);
    if (kesPeriodStr == NULL) {
        TRACE("Failed to allocate kesPeriodStr");
        opcert_buffer_cleanup();
        return io_send_sw(SW_INSUFFICIENT_MEMORY);
    }
    if (!format_u64(kesPeriodStr, MAX_UINT64_STRING_SIZE, opcert->kesPeriod)) {
        TRACE("Failed to format KES period");
        opcert_buffer_cleanup();
        return io_send_sw(SW_DISPLAY_AMOUNT_FAIL);
    }

    // Allocate and fill issue counter
    issueCounterStr = (char *) ui_mem_alloc(MAX_UINT64_STRING_SIZE);
    if (issueCounterStr == NULL) {
        TRACE("Failed to allocate issueCounterStr");
        opcert_buffer_cleanup();
        return io_send_sw(SW_INSUFFICIENT_MEMORY);
    }
    if (!format_u64(issueCounterStr, MAX_UINT64_STRING_SIZE, opcert->issueCounter)) {
        TRACE("Failed to format issue counter");
        opcert_buffer_cleanup();
        return io_send_sw(SW_DISPLAY_AMOUNT_FAIL);
    }

    // Setup data to display
    if (!ui_pairs_init(5)) {
        TRACE("Failed to initialize pairs");
        opcert_buffer_cleanup();
        return io_send_sw(SW_DISPLAY_AMOUNT_FAIL);
        // TODO not sure if this is sufficient or some other "ui_after_error" should be called
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
    switch (securityPolicy) {
        case POLICY_PROMPT_WARN_UNUSUAL:
            TRACE("Setting up warning for POLICY_PROMPT_WARN_UNUSUAL");
            // Allocate warning structure dynamically
            g_warning = (nbgl_warning_t *) ui_mem_alloc(sizeof(nbgl_warning_t));
            if (g_warning == NULL) {
                TRACE("Failed to allocate warning structure");
                opcert_buffer_cleanup();
                return io_send_sw(SW_INSUFFICIENT_MEMORY);
            }
            // TODO not sure about proper icons
            g_warning->introDetails = &warningDetails;
            g_warning->reviewDetails = &warningDetails;
            g_warning->info = &warningInfo;
            g_warning->introTopRightIcon = &WARNING_ICON;
            g_warning->reviewTopRightIcon = &WARNING_ICON;
            warningPtr = g_warning;
            break;

        case POLICY_PROMPT_BEFORE_RESPONSE:
            TRACE("NO WARNING - POLICY_PROMPT_BEFORE_RESPONSE");
            break;

        default:
            TRACE("UNEXPECTED SECURITY POLICY: %d", securityPolicy);
            // Catch any truly unknown or unexpected policy values.
            ASSERT(false);
            break;
    }

    nbgl_useCaseAdvancedReview(TYPE_OPERATION,
                        g_pairsList,
                        &ICON_APP_CARDANO,
                        "Sign operational\ncertificate",
                        NULL,
                        "Sign certificate",
                        NULL,
                        warningPtr,
                        review_choice
    );

    return 0;
}
