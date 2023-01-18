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

#pragma GCC diagnostic ignored "-Wformat-invalid-specifier"  // snprintf
#pragma GCC diagnostic ignored "-Wformat-extra-args"         // snprintf

#include <stdbool.h>  // bool
#include <string.h>   // memset

#include "os.h"
#include "glyphs.h"
#include "nbgl_use_case.h"

#include "display.h"
#include "constants.h"
#include "../globals.h"
#include "../io.h"
#include "../sw.h"
#include "../address.h"
#include "action/validate.h"
#include "../transaction/types.h"
#include "../common/bip32.h"
#include "../common/format.h"
#include "../menu.h"

static char g_address[43];
static char g_bip32_path[60];

static nbgl_layoutTagValue_t pairs[1];
static nbgl_layoutTagValueList_t pairList;

static void confirm_address_rejection(void) {
    // display a status page and go back to main
    validate_pubkey(false);
    nbgl_useCaseStatus("Address verification\ncancelled", false, ui_menu_main);
}

static void confirm_address_approval(void) {
    // display a success status page and go back to main
    validate_pubkey(true);
    nbgl_useCaseStatus("ADDRESS\nVERIFIED", true, ui_menu_main);
}

static void review_choice(bool confirm) {
    if (confirm) {
        confirm_address_approval();
    } else {
        confirm_address_rejection();
    }
}

static void continue_review(void) {
    // Fill pairs
    pairs[0].item = "BIP32 Path";
    pairs[0].value = g_bip32_path;

    pairList.nbMaxLinesForValue = 0;
    pairList.nbPairs = 1;
    pairList.pairs = pairs;
    nbgl_useCaseAddressConfirmationExt(g_address, review_choice, &pairList);
}

int ui_display_address() {
    if (G_context.req_type != CONFIRM_ADDRESS || G_context.state != STATE_NONE) {
        G_context.state = STATE_NONE;
        return io_send_sw(SW_BAD_STATE);
    }

    memset(g_bip32_path, 0, sizeof(g_bip32_path));
    if (!bip32_path_format(G_context.bip32_path,
                           G_context.bip32_path_len,
                           g_bip32_path,
                           sizeof(g_bip32_path))) {
        return io_send_sw(SW_DISPLAY_BIP32_PATH_FAIL);
    }

    memset(g_address, 0, sizeof(g_address));
    uint8_t address[ADDRESS_LEN] = {0};
    if (!address_from_pubkey(G_context.pk_info.raw_public_key, address, sizeof(address))) {
        return io_send_sw(SW_DISPLAY_ADDRESS_FAIL);
    }
    snprintf(g_address, sizeof(g_address), "0x%.*H", sizeof(address), address);

    nbgl_useCaseReviewStart(&C_stax_app_boilerplate_64px,
                            "Verify BOL address",
                            NULL,
                            "Cancel",
                            continue_review,
                            confirm_address_rejection);
    return 0;
}

#endif
