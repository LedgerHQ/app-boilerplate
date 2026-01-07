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

#include <stdint.h>   // uint*_t
#include <stdbool.h>  // bool
#include <stddef.h>   // size_t
#include <string.h>   // memset, explicit_bzero

#include "os.h"
#include "cx.h"
#include "io.h"
#include "buffer.h"
#include "crypto_helpers.h"
#include "nbgl_use_case.h"

#include "get_public_key.h"
#include "globals.h"
#include "keyDerivation.h"
#include "utils/utils.h"
#include "utils/cardano_os_utils.h"
#include "cardano_swo.h"
#include "display.h"
#include "dispatcher.h"
#include "securityPolicy.h"
#include "nbgl_use_case.h"
#include "menu.h"

int handler_get_public_key(buffer_t *cdata) {
    TRACE();
    explicit_bzero(&G_context, sizeof(G_context));
    G_context.req_type = REQUEST_EXPORT_PUBKEY;

    if (!buffer_read_bip44_path(cdata, &G_context.pk_info.path)) {
        TRACE();
        return send_error_and_reset(SWO_BIP44_PATH_PARSING_FAIL);
    }

    // Log the requested path for easier debugging.
    BIP44_PRINTF(&G_context.pk_info.path);

    // Check security policy
    warning_bits_t warnings = {0};
    warning_bits_init(&warnings);
    security_policy_t policy = policyForGetExtendedPublicKey(&G_context.pk_info.path, &warnings);
    TRACE("Security policy: %d", (int) policy);
    // maybe not here? TODO
    if (policy == POLICY_DENY) {
        TRACE("Security policy DENY - rejecting operation");
        TRACE("Calling nbgl_useCaseStatus(\"Export of public key denied\", false, ui_menu_main)");
        nbgl_useCaseStatus("Export of public key denied", false, ui_menu_main);
        return send_error_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
    }

    {
        cx_err_t error = deriveExtendedPublicKey(&G_context.pk_info.path, &G_context.pk_info.extPubKey);
        if (error != CX_OK) {
            return send_error_and_reset(error);
        }
    }

    return ui_display_pubkey(policy, warnings);
}

void finalize_pubkey_export(bool confirmed) {
    TRACE("confirmed = %d", confirmed);

    if (!confirmed) {
        G_context.req_type = REQUEST_NONE;  // Reset to idle
        io_send_sw(SWO_CONDITIONS_NOT_SATISFIED);
        return;
    }

    ASSERT(G_context.req_type == REQUEST_EXPORT_PUBKEY);

    // Send the extended public key back to the client
    io_send_response_pointer((uint8_t*) &G_context.pk_info.extPubKey, SIZEOF(G_context.pk_info.extPubKey), SWO_SUCCESS);
    G_context.req_type = REQUEST_NONE;  // Reset to idle after sending
}
