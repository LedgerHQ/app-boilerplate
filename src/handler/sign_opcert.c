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
#include "buffer.h"
#include "nbgl_use_case.h"

#include "cardano_swo.h"
#include "globals.h"
#include "utils/utils.h"
#include "utils/textUtils.h"
#include "utils/cardano_os_utils.h"
#include "display.h"
#include "opcert_types.h"
#include "opcert_parse.h"
#include "securityPolicy.h"
#include "messageSigning.h"
#include "buffer_utils.h"
#include "addressUtils/bip44.h"
#include "write.h"
#include "sign_opcert.h"
#include "menu.h"

#define OP_CERT_BODY_LENGTH (KES_PUBLIC_KEY_LENGTH + 8 + 8)

int handler_sign_opcert(buffer_t *cdata) {
    TRACE_BUFFER(cdata->ptr, cdata->size);

    explicit_bzero(&G_context, sizeof(G_context));
    G_context.req_type = REQUEST_SIGN_OPCERT;
    G_context.state.opcert_state = OPCERT_STATE_NONE;

    G_context.opcert_info.raw_opcert_len = cdata->size;
    if (!buffer_move(cdata, G_context.opcert_info.raw_opcert, sizeof(G_context.opcert_info.raw_opcert))) {
        return send_error_and_reset(SWO_INVALID_OPCERT_LENGTH);
    }

    buffer_t buf = {.ptr = G_context.opcert_info.raw_opcert,
                    .size = G_context.opcert_info.raw_opcert_len,
                    .offset = 0};
    TRACE_BUFFER(buf.ptr, buf.size);

    opcert_parser_status_e status = parse_opcert(&buf, &G_context.opcert_info.opcert);
    TRACE("Opcert parsing status: %d\n", status);
    if (status != PARSING_OK) {
        return send_error_and_reset(opcert_map_parser_status_to_swo(status));
    }
    G_context.state.opcert_state = OPCERT_STATE_PARSED;
    const parsed_opcert_t* opcert = &G_context.opcert_info.opcert;

    // Log parsed opcert details (path, KES period, issue counter)
    BIP44_PRINTF(&opcert->poolColdKeyPath);
    TRACE("KES period = ");
    TRACE_UINT64(opcert->kesPeriod);
    TRACE("issue counter = ");
    TRACE_UINT64(opcert->issueCounter);

    // Check security policy
    warning_bits_t warnings = 0;
    warning_bits_init(&warnings);
    security_policy_t policy = policyForSignOpCert(&opcert->poolColdKeyPath, &warnings);
    TRACE("Security policy: %d\n", policy);
    if (policy == POLICY_DENY) {
        TRACE("Security policy DENY - rejecting operation");
        TRACE("Calling nbgl_useCaseStatus(\"Operational certificate denied\", false, ui_menu_main)");
        nbgl_useCaseStatus("Operational certificate denied", false, ui_menu_main);
        // TODO make sure the constants are defined in a proper place
        return send_error_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
    }

    ui_display_opcert(policy, warnings);

    return 0;
}

void finalize_sign_opcert(bool confirmed) {
    if (!confirmed) {
        G_context.state.opcert_state = OPCERT_STATE_NONE;
        G_context.req_type = REQUEST_NONE;  // Reset to idle
        io_send_sw(SWO_CONDITIONS_NOT_SATISFIED);
        return;
    }

    // user confirmed
    G_context.state.opcert_state = OPCERT_STATE_APPROVED;

    // assemble the opcert bytestring and sign it
    const parsed_opcert_t* opcert = &G_context.opcert_info.opcert;
    uint8_t opCertBodyBuffer[OP_CERT_BODY_LENGTH] = {0};
    explicit_bzero(opCertBodyBuffer, SIZEOF(opCertBodyBuffer));
    {
        write_buffer_t buf = buffer_init(opCertBodyBuffer, SIZEOF(opCertBodyBuffer));

        // Buffer is exactly sized - failure is programming error
        ASSERT(buffer_write_bytes(&buf,
                                  (const uint8_t*) opcert->kesPublicKey,
                                  KES_PUBLIC_KEY_LENGTH));
        ASSERT(buffer_write_u64(&buf, opcert->issueCounter, BE));
        ASSERT(buffer_write_u64(&buf, opcert->kesPeriod, BE));

        ASSERT(buffer_written_size(&buf) == OP_CERT_BODY_LENGTH);
        TRACE_BUFFER(opCertBodyBuffer, SIZEOF(opCertBodyBuffer));
    }

    ASSERT(bip44_isPoolColdKeyPath(&opcert->poolColdKeyPath));
    int r = signRawMessageWithPath(
        &opcert->poolColdKeyPath,
        opCertBodyBuffer, SIZEOF(opCertBodyBuffer),
        G_context.opcert_info.signature, SIZEOF(G_context.opcert_info.signature)
    );

    if (r != 0) {
        G_context.state.opcert_state = OPCERT_STATE_NONE;
        G_context.req_type = REQUEST_NONE;  // Reset to idle
        io_send_sw(SWO_SIGNATURE_FAIL);
    } else {
        io_send_response_pointer(
            G_context.opcert_info.signature,
            SIZEOF(G_context.opcert_info.signature),
            SWO_SUCCESS
        );
        G_context.state.opcert_state = OPCERT_STATE_NONE;  // Reset to idle after sending
        G_context.req_type = REQUEST_NONE;
    }
}
