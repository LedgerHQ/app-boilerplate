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
#include "cardano_settings.h"

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
    const size_t pubkeyPathStrSize = MAX_BIP44_PATH_STRING_LENGTH + 1;
    pubkeyPathStr = (char *) ui_mem_alloc(pubkeyPathStrSize);
    if (pubkeyPathStr == NULL) {
        ui_cleanup_tracked_allocations();
        return send_error_and_reset(SWO_INSUFFICIENT_MEMORY);
    }
    explicit_bzero(pubkeyPathStr, pubkeyPathStrSize);
    bool pathFormatted = format_bip44_path(&pk->path, pubkeyPathStr, pubkeyPathStrSize);
    ASSERT(pathFormatted);
    ASSERT(strlen(pubkeyPathStr) + 1 < pubkeyPathStrSize);

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

    bool isColdKey = (bip44_classifyPath(&pk->path) == PATH_POOL_COLD_KEY);
    const char* keyTypeLabel = isColdKey ? "Cold public key" : "Public key";

    // Prepare icon and title based on whether path is unusual
    bool isUnusual = warning_bits_has(warnings, WARNING_BIT_UNUSUAL_KEY_DERIVATION_PATH);
    const nbgl_icon_details_t* icon = isUnusual ? &WARNING_ICON : &ICON_APP_CARDANO;
    const char* exportPrefix = isUnusual ? "Export UNUSUAL" : "Export";

    char title[64] = {0};
    explicit_bzero(title, sizeof(title));
    snprintf(title, sizeof(title), "%s %s", exportPrefix, keyTypeLabel);

    ASSERT(strlen(title) > 0);
    ASSERT(strlen(title) + 1 < SIZEOF(title));
    ASSERT(icon != NULL);

    nbgl_useCaseChoice(
                        icon,
                        title,
                        pubkeyPathStr,
                        "Export",
                        "Reject",
                        pubkey_review_choice
    );

    return 0;
}
