/*****************************************************************************
 *   Ledger App NBGL_Tests.
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
#include "os_io_seproxyhal.h"
#include "nbgl_use_case.h"
#include "io.h"
#include "bip32.h"
#include "format.h"

#include "display.h"
#include "constants.h"
#include "../globals.h"
#include "../sw.h"
#include "action/validate.h"
#include "../transaction/types.h"
#include "../menu.h"

// Buffer where the transaction amount string is written
static char g_amount[30];
// Buffer where the transaction address string is written
static char g_address[43];

static nbgl_layoutTagValue_t pairs[2];
static nbgl_layoutTagValueList_t pairList;

// called when long press button on 3rd page is long-touched or when reject footer is touched
static void review_choice(bool confirm) {
    // Answer, display a status page and go back to main
    validate_transaction(confirm);
    if (confirm) {
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, ui_menu_main);
    } else {
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_REJECTED, ui_menu_main);
    }
}

// Public function to start the transaction review
// - Check if the app is in the right state for transaction review
// - Format the amount and address strings in g_amount and g_address buffers
// - Display the first screen of the transaction review
int ui_display_transaction() {
    // Format amount and address to g_amount and g_address buffers
    memset(g_amount, 0, sizeof(g_amount));
    snprintf(g_amount, sizeof(g_amount), "BOL 0.99");
    memset(g_address, 0, sizeof(g_address));
    snprintf(g_amount, sizeof(g_amount), "0x1234567890");

    // Setup data to display
    pairs[0].item = "Amount";
    pairs[0].value = g_amount;
    pairs[1].item = "Address";
    pairs[1].value = g_address;

    // Setup list
    pairList.nbMaxLinesForValue = 0;
    pairList.nbPairs = 2;
    pairList.pairs = pairs;

    // Start review
    nbgl_useCaseReview(TYPE_TRANSACTION,
                       &pairList,
                       &LARGE_ICON,
                       "Review transaction\nto send BOL",
                       NULL,
                       "Sign transaction\nto send BOL",
                       review_choice);
    return 0;
}

#endif
