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
#include "format.h"

#include "display.h"
#include "constants.h"
#include "globals.h"
#include "utils/utils.h"
#include "utils/cardano_os_utils.h"
#include "cardano_swo.h"
#include "opcert_types.h"
#include "menu.h"
#include "securityPolicy.h"
#include "nbgl_screens.h"
#include "get_public_key.h"
#include "memory/mem_utils.h"
#include "ui_utils.h"
#include "settings.h"

static char *pubkeyPathStr = NULL;

static void pubkey_review_choice(bool confirm) {
    // Cleanup display buffers
    ui_cleanup_tracked_allocations();

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

int ui_display_pubkey(security_policy_t securityPolicy, warning_bits_t warnings) {
    TRACE("=== ui_display_pubkey START ===");
    TRACE("securityPolicy: %d", securityPolicy);

    if (G_context.req_type != REQUEST_EXPORT_PUBKEY) {
        TRACE("Bad request type detected - returning error");
        return send_error_and_reset(SWO_BAD_STATE);
    }

    pubkey_ctx_t* pk = &G_context.pk_info;

    // Allocate display buffers
    pubkeyPathStr = (char *) ui_mem_alloc(BIP44_PATH_STRING_SIZE_MAX + 1);
    if (pubkeyPathStr == NULL) {
        ui_cleanup_tracked_allocations();
        return send_error_and_reset(SWO_INSUFFICIENT_MEMORY);
    }
    ui_getPathScreen(pubkeyPathStr, BIP44_PATH_STRING_SIZE_MAX + 1, &pk->path);

    // set warning if needed
    bool isUnusual = warning_bits_has(warnings, WARNING_BIT_UNUSUAL_KEY_DERIVATION_PATH);

    switch (securityPolicy) {
        case POLICY_SHOW:
            pk->silentExport = false;
            break;

        case POLICY_HIDE:
            ASSERT(is_silent_pubkey_export_allowed());
            pk->silentExport = true;
            finalize_pubkey_export(true);
            ui_cleanup_tracked_allocations();
            return 0;

        default:
            ASSERT(false);
            ui_cleanup_tracked_allocations();
            return 0;
    }
    TRACE("isUnusual: %d", isUnusual);

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
                            pubkey_review_choice
        );
    } else {
        nbgl_useCaseChoice(
                            &ICON_APP_CARDANO,
                            "Export public key",
                            pubkeyPathStr,
                            "Export",
                            "Reject",
                            pubkey_review_choice
        );
    }

    return 0;
}
