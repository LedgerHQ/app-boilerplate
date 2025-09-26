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
#include "get_public_key.h"

static char pubkeyPathStr[BIP44_PATH_STRING_SIZE_MAX + 1];

static void review_choice(bool confirm) {
    // Answer, display a status page and go back to main
    finalize_pubkey_export(confirm);

    if (!G_context.pk_info.silentExport) {
        if (confirm) {
            nbgl_useCaseStatus("Public key\nexported", true, ui_menu_main);
        } else {
            nbgl_useCaseStatus("Public key\ndenied", true, ui_menu_main);
        }
    } else {
        ui_menu_main();
    }
}

int ui_display_pubkey(security_policy_t securityPolicy) {
    TRACE("=== ui_display_pubkey START ===");
    TRACE("securityPolicy: %d", securityPolicy);

    if (G_context.req_type != REQUEST_EXPORT_PUBKEY || G_context.state != STATE_PARSED) {
        TRACE("Bad state detected - returning error");
        G_context.state = STATE_NONE;
        return io_send_sw(SW_BAD_STATE);
    }

    pubkey_ctx_t* pk = &G_context.pk_info;

    // set warning if needed
    bool isUnusual = false;
    switch (securityPolicy) {
        case POLICY_PROMPT_WARN_UNUSUAL:
            pk->silentExport = false;
            isUnusual = true;
            break;

        case POLICY_PROMPT_BEFORE_RESPONSE:
            pk->silentExport = false;
            // no warning, nothing to do
            break;

        case POLICY_ALLOW_WITHOUT_PROMPT:
            pk->silentExport = true;
            finalize_pubkey_export(true);
            return 0;

        default:
            // Catch any truly unknown or unexpected policy values.
            ASSERT(false);
            return 0;
    }
    TRACE("isUnusual: %d", isUnusual);

    ui_getPathScreen(pubkeyPathStr, SIZEOF(pubkeyPathStr), &pk->path);

    if (isUnusual) {
        // a mild warning about unusual path
        // no immediate threat, just to be aware that the client (SW wallet)
        // behaves in an unusual way
        nbgl_useCaseChoice(
                            &WARNING_ICON,
                            "Export UNUSUAL public key",
                            pubkeyPathStr,
                            "Export",
                            "Reject",
                            review_choice
        );
    } else {
        nbgl_useCaseChoice(
                            &ICON_APP_CARDANO,
                            "Export public key",
                            pubkeyPathStr,
                            "Export",
                            "Reject",
                            review_choice
        );
    }

    return 0;
}
