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
#include "io.h"
#include "bip32.h"
#include "format.h"

#include "display.h"
#include "constants.h"
#include "../globals.h"
#include "../sw.h"
#include "../address.h"
#include "action/validate.h"
#include "../transaction/types.h"
#include "../menu.h"

int ui_display_address() {
    if (G_context.req_type != CONFIRM_ADDRESS || G_context.state != STATE_NONE) {
        G_context.state = STATE_NONE;
        return io_send_sw(SW_BAD_STATE);
    }

    char address_str[43] = {0};
    uint8_t address_bin[ADDRESS_LEN] = {0};

    if (!address_from_pubkey(G_context.pk_info.raw_public_key, address_bin, sizeof(address_bin))) {
        return io_send_sw(SW_DISPLAY_ADDRESS_FAIL);
    }

    if (format_hex(address_bin, sizeof(address_bin), address_str, sizeof(address_str)) == -1) {
        return io_send_sw(SW_DISPLAY_ADDRESS_FAIL);
    }

    sync_nbgl_ret_t ret = sync_nbgl_useCaseAddressReview(address_str,
                                                         &C_app_boilerplate_64px,
                                                         "Verify BOL address",
                                                         NULL);

    if (ret == NBGL_SYNC_RET_SUCCESS) {
        // display a status page and go back to main
        validate_pubkey(true);
        nbgl_useCaseStatus("ADDRESS\nVERIFIED", true, ui_menu_main);
    } else if (ret == NBGL_SYNC_RET_REJECTED) {
        // display a status page and go back to main
        validate_pubkey(false);
        nbgl_useCaseStatus("Address verification\ncancelled", false, ui_menu_main);
    } else {
        io_send_sw(SW_BAD_STATE);
        nbgl_useCaseStatus("Address verification\nissue", false, ui_menu_main);
    }

    // Here we used async version of nbgl_useCaseStatus
    // This means that upon end of timer or touch ui_menu_main will be called
    // but in the meantime we return to app_main to process APDU.
    // If an APDU is received before the timer ends, a new UX might be shown on
    // the screen, and therefore the modal will be dismissed and its callback
    // will never be called.
    return 0;
}

#endif
