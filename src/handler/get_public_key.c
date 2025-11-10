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
#include "types.h"
#include "sw.h"
#include "display.h"
#include "send_response.h"
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
        return io_send_sw(SW_BIP44_PATH_PARSING_FAIL);
    }

    // Check security policy
    security_policy_t policy = policyForGetExtendedPublicKey(&G_context.pk_info.path);
    TRACE("Security policy: %d", (int) policy);
    // maybe not here? TODO
    if (policy == POLICY_DENY) {
        TRACE("Security policy DENY - rejecting operation");
        nbgl_useCaseStatus("Export of public key denied", false, ui_menu_main);
        // TODO make sure the constants are defined properly
        return io_send_sw(ERR_REJECTED_BY_POLICY);
    }

    {
        cx_err_t error = deriveExtendedPublicKey(&G_context.pk_info.path, &G_context.pk_info.extPubKey);
        if (error != CX_OK) {
            return io_send_sw(error);
        }
    }

    return ui_display_pubkey(policy);
}

void finalize_pubkey_export(bool confirmed) {
    TRACE("confirmed = %d", confirmed);

    if (!confirmed) {
        io_send_sw(SW_DENY);
        return;
    }

    ASSERT(G_context.req_type == REQUEST_EXPORT_PUBKEY);

    // Send the extended public key back to the client
    io_send_response_pointer((uint8_t*) &G_context.pk_info.extPubKey, SIZEOF(G_context.pk_info.extPubKey), SW_OK);
}
