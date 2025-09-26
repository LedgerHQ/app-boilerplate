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

static char poolColdKeyPathStr[BIP44_PATH_STRING_SIZE_MAX + 1];
static char poolKeyHashStr[BECH32_STRING_SIZE_MAX];
static char kesKeyStr[BECH32_STRING_SIZE_MAX];
static char kesPeriodString[MAX_UINT64_STRING_SIZE];
static char issueCounterString[MAX_UINT64_STRING_SIZE];

// Items:
// pool cold key
// pool id
// KES public key
// KES period
// issue counter
static nbgl_contentTagValue_t pairs[5];
static nbgl_contentTagValueList_t pairList;

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

static nbgl_warning_t warning = {0};

// called when long press button on 3rd page is long-touched or when reject footer is touched
static void review_choice(bool confirm) {
    TRACE("=== review_choice called ===");
    TRACE("confirm: %s", confirm ? "true" : "false");

    // Answer, display a status page and go back to main
    finalize_sign_opcert(confirm);

    if (confirm) {
        TRACE("User confirmed - showing signed status");
        nbgl_useCaseReviewStatus(STATUS_TYPE_OPERATION_SIGNED, ui_menu_main);
    } else {
        TRACE("User rejected - showing rejected status");
        nbgl_useCaseReviewStatus(STATUS_TYPE_OPERATION_REJECTED, ui_menu_main);
    }
    TRACE("=== review_choice end ===");
}

// Public function to start the transaction review
// - Check if the app is in the right state for transaction review
// - Format the amount and address strings in g_amount and g_address buffers
// - Display the first screen of the transaction review
// - Display a warning if the transaction is blind-signed
int ui_display_opcert(security_policy_t securityPolicy) {
    TRACE("=== ui_display_opcert START ===");
    TRACE("securityPolicy: %d", securityPolicy);

    if (G_context.req_type != REQUEST_SIGN_OPCERT || G_context.state != STATE_PARSED) {
        TRACE("Bad state detected - returning error");
        G_context.state = STATE_NONE;
        return io_send_sw(SW_BAD_STATE);
    }

    const parsed_opcert_t* opcert = &G_context.opcert_info.opcert;

    // pool cold key
    ui_getPathScreen(poolColdKeyPathStr, SIZEOF(poolColdKeyPathStr), &opcert->poolColdKeyPath);

    // pool id
    uint8_t poolKeyHash[POOL_KEY_HASH_LENGTH] = {0};
    bip44_pathToKeyHash(&opcert->poolColdKeyPath, poolKeyHash, SIZEOF(poolKeyHash));
    ui_getBech32Screen(poolKeyHashStr,
                        SIZEOF(poolKeyHashStr),
                        "pool",
                        poolKeyHash,
                        SIZEOF(poolKeyHash));

    // KES public key
    ui_getBech32Screen(kesKeyStr,
                        SIZEOF(kesKeyStr),
                        "kes_vk",
                        opcert->kesPublicKey,
                        KES_PUBLIC_KEY_LENGTH);

    // KES period
    explicit_bzero(kesPeriodString, SIZEOF(kesPeriodString));
    if (!format_u64(kesPeriodString, SIZEOF(kesPeriodString), opcert->kesPeriod)) {
        // TODO perhaps just assert since this is a bug of not enough memory allocated
        return io_send_sw(SW_DISPLAY_AMOUNT_FAIL);
    }

    // issue counter
    explicit_bzero(issueCounterString, SIZEOF(issueCounterString));
    if (!format_u64(issueCounterString, SIZEOF(issueCounterString), opcert->issueCounter)) {
        // TODO perhaps just assert since this is a bug of not enough memory allocated
        return io_send_sw(SW_DISPLAY_AMOUNT_FAIL);
    }

    // Setup data to display
    pairs[0].item = "Pool cold key path";
    pairs[0].value = poolColdKeyPathStr;
    pairs[1].item = "Pool ID";
    pairs[1].value = poolKeyHashStr;
    pairs[2].item = "KES public key";
    pairs[2].value = kesKeyStr;
    pairs[3].item = "KES period";
    pairs[3].value = kesPeriodString;
    pairs[4].item = "Issue counter";
    pairs[4].value = issueCounterString;

    // Setup list
    pairList.nbMaxLinesForValue = 0;
    pairList.nbPairs = 5;
    pairList.pairs = pairs;

    // set warning if needed
    const nbgl_warning_t* warningPtr = NULL;
    TRACE("Security policy received: %d", securityPolicy);
    switch (securityPolicy) {
        case POLICY_PROMPT_WARN_UNUSUAL:
            TRACE("Setting up warning for POLICY_PROMPT_WARN_UNUSUAL");
            explicit_bzero(&warning, sizeof(nbgl_warning_t));
            // TODO not sure about proper icons
            warning.introDetails = &warningDetails;
            warning.reviewDetails = &warningDetails;
            warning.info = &warningInfo;
            warning.introTopRightIcon = &WARNING_ICON;
            warning.reviewTopRightIcon = &WARNING_ICON;
            warningPtr = &warning;
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
                        &pairList,
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
