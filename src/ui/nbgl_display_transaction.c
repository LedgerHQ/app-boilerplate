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

#ifdef HAVE_NBGL

#include <stdbool.h>  // bool
#include <string.h>   // memset

#include "os.h"
#include "glyphs.h"
#include "nbgl_use_case.h"
#include "nbgl_sync.h"
#include "format.h"

#include "constants.h"
#include "display.h"
#include "address.h"
#include "menu.h"

// Public function to start the transaction review
// - Check if the app is in the right state for transaction review
// - Format the amount and address strings in amount_str and address_str buffers
// - Display the first screen of the transaction review
ui_ret_e ui_display_transaction(const transaction_t *transaction) {

    // Buffer where the transaction amount string is written
    char amount_str[30] = {0};
    // Buffer where the transaction address string is written
    char address_str[43] = {0};

    // Format amount and address to amount_str and address_str buffers
    char amount_bin[30] = {0};
    if (!format_fpu64(amount_bin,
                      sizeof(amount_bin),
                      transaction->value,
                      EXPONENT_SMALLEST_UNIT)) {
        return UI_RET_FAILURE;
    }
    snprintf(amount_str, sizeof(amount_str), "BOL %.*s", sizeof(amount_bin), amount_bin);

    if (format_hex(transaction->to, ADDRESS_LEN, address_str, sizeof(address_str)) ==
        -1) {
        return UI_RET_FAILURE;
    }

    nbgl_layoutTagValue_t pairs[2] = {0};
    nbgl_layoutTagValueList_t pairList = {0};

    // Setup data to display
    pairs[0].item = "Amount";
    pairs[0].value = amount_str;
    pairs[1].item = "Address";
    pairs[1].value = address_str;

    // Setup list
    pairList.nbMaxLinesForValue = 0;
    pairList.nbPairs = 2;
    pairList.pairs = pairs;

    // Start review
    sync_nbgl_ret_t ret = sync_nbgl_useCaseTransactionReview(
        &pairList,
        &C_app_boilerplate_64px,
        "Review transaction\nto send BOL",
        NULL,
        "Sign transaction\nto send BOL");

    if (ret == NBGL_SYNC_RET_SUCCESS) {
        return UI_RET_APPROVED;
    } else if (ret == NBGL_SYNC_RET_REJECTED) {
        return UI_RET_REJECTED;
    } else {
        return UI_RET_FAILURE;
    }
}

void ui_display_transaction_status(ui_ret_e ret) {
    // Here we use sync version of nbgl_useCaseStatus
    // This means that upon reception of any APDU during
    // sync_nbgl_useCaseStatus, we will stop the status display even if the
    // received APDU doesn't need an UX flow to be answered.

    if (ret == UI_RET_APPROVED) {
        sync_nbgl_useCaseStatus("TRANSACTION\nSIGNED", true);
    } else if (ret == UI_RET_REJECTED) {
        sync_nbgl_useCaseStatus("Transaction rejected", false);
    } else {
        sync_nbgl_useCaseStatus("Transaction issue", false);
    }

    ui_menu_main();
}

#endif
