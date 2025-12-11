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
#include "tokens.h"

#include "display.h"
#include "constants.h"
#include "globals.h"
#include "sw.h"
#include "address.h"
#include "validate.h"
#include "tx_types.h"
#include "menu.h"

// Buffer where the transaction amount string is written
static char g_amount[30];
// Buffer where the transaction address string is written
static char g_address[43];
// Buffer where the token ticker is written
static char g_token[10];

// The flow with the most pairs to display is the token signing flow with amount + dest + token
static nbgl_contentTagValue_t pairs[3];
static nbgl_contentTagValueList_t pairList;

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
// - Format the token name if in token signing flow
// - Display the first screen of the transaction review
// - Display a warning if the transaction is blind-signed
static int ui_display_transaction_bs_token_choice(bool is_blind_signing, bool is_token_signing) {
    explicit_bzero(g_amount, sizeof(g_amount));
    explicit_bzero(g_address, sizeof(g_address));
    explicit_bzero(g_token, sizeof(g_token));

    // Validate state based on transaction type
    uint8_t expected_req_type;
    if (is_token_signing) {
        expected_req_type = CONFIRM_TOKEN_TRANSACTION;
    } else {
        expected_req_type = CONFIRM_TRANSACTION;
    }
    if (G_context.req_type != expected_req_type || G_context.state != STATE_PARSED) {
        G_context.state = STATE_NONE;
        PRINTF("ui_display_transaction: bad state %d/%d (expected %d/%d)\n",
               G_context.req_type,
               G_context.state,
               expected_req_type,
               STATE_PARSED);
        return io_send_sw(SWO_CONDITIONS_NOT_SATISFIED);
    }

    // Format amount
    // 50 chars is comfortable for amount formatting
    char amount[50 + MAX_TICKER_SIZE] = {0};
    uint8_t decimals =
        is_token_signing ? G_context.tx_info.token_info.decimals : EXPONENT_SMALLEST_UNIT;
    if (!format_fpu64(amount, sizeof(amount), G_context.tx_info.transaction.value, decimals)) {
        return io_send_sw(SWO_INCORRECT_DATA);
    }

    if (is_token_signing) {
        snprintf(g_amount,
                 sizeof(g_amount),
                 "%.*s %s",
                 (int) strlen(amount),
                 amount,
                 G_context.tx_info.token_info.ticker);
    } else {
        snprintf(g_amount, sizeof(g_amount), "%.*s BOL", (int) strlen(amount), amount);
    }

    // Format address
    if (format_hex(G_context.tx_info.transaction.to, ADDRESS_LEN, g_address, sizeof(g_address)) ==
        -1) {
        return io_send_sw(SWO_INCORRECT_DATA);
    }

    // Setup pairs based on transaction type
    int pair_index = 0;
    if (is_token_signing) {
        // Token flow: Token, Amount, Address
        snprintf(g_token, sizeof(g_token), "%s", G_context.tx_info.token_info.ticker);
        pairs[pair_index].item = "Token";
        pairs[pair_index].value = g_token;
        pair_index++;
    }

    pairs[pair_index].item = "Amount";
    pairs[pair_index].value = g_amount;
    pair_index++;

    pairs[pair_index].item = "To";
    pairs[pair_index].value = g_address;
    pair_index++;

    // Setup list
    pairList.nbMaxLinesForValue = 0;
    pairList.nbPairs = pair_index;
    pairList.pairs = pairs;
    pairList.wrapping = true;

    // Determine review text
    const char *review_text =
        is_token_signing ? "Review transaction\nto send token" : "Review transaction\nto send BOL";
#ifdef SCREEN_SIZE_WALLET
    // On big screen size devices, display a rich text with some context on the final page
    const char *sign_text =
        is_token_signing ? "Sign transaction\nto send token?" : "Sign transaction\nto send BOL?";
#else
    // On small screen size devices, fallback to the SDK default text
    const char *sign_text = NULL;
#endif

    if (is_blind_signing) {
        // Start blind-signing review flow
        nbgl_useCaseReviewBlindSigning(TYPE_TRANSACTION,
                                       &pairList,
                                       &ICON_APP_BOILERPLATE,
                                       review_text,
                                       NULL,
                                       sign_text,
                                       NULL,
                                       review_choice);
    } else {
        // Start review flow
        nbgl_useCaseReview(TYPE_TRANSACTION,
                           &pairList,
                           &ICON_APP_BOILERPLATE,
                           review_text,
                           NULL,
                           sign_text,
                           review_choice);
    }
    return 0;
}

// Flow used to display a blind-signed transaction
int ui_display_blind_signed_transaction(void) {
    return ui_display_transaction_bs_token_choice(true, false);
}

// Flow used to display a clear-signed transaction
int ui_display_transaction(void) {
    return ui_display_transaction_bs_token_choice(false, false);
}

// Public function to start the token transaction review
int ui_display_token_transaction(void) {
    return ui_display_transaction_bs_token_choice(false, true);
}
