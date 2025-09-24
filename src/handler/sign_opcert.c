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
#include "buffer.h"
#include "swap.h"
#include "nbgl_use_case.h"

#include "sw.h"
#include "globals.h"
#include "display.h"
#include "opcert_types.h"
#include "parse_opcert.h"
#include "validate.h"
#include "securityPolicy.h"
#include "messageSigning.h"
#include "bufView.h"
#include "write.h"
#include "sign_opcert.h"
#include "menu.h"

#define OP_CERT_BODY_LENGTH (KES_PUBLIC_KEY_LENGTH + 8 + 8)

int handler_sign_opcert(buffer_t *cdata) {
    TRACE_BUFFER(cdata->ptr, cdata->size);

    explicit_bzero(&G_context, sizeof(G_context));
    G_context.req_type = REQUEST_SIGN_OPCERT;
    G_context.state = STATE_NONE;

    G_context.opcert_info.raw_opcert_len = cdata->size;
    if (!buffer_move(cdata, G_context.opcert_info.raw_opcert, sizeof(G_context.opcert_info.raw_opcert))) {
        return io_send_sw(SW_WRONG_OPCERT_LENGTH);
    }

    buffer_t buf = {.ptr = G_context.opcert_info.raw_opcert,
                    .size = G_context.opcert_info.raw_opcert_len,
                    .offset = 0};
    TRACE_BUFFER(buf.ptr, buf.size);

    opcert_parser_status_e status = opcert_deserialize(&buf, &G_context.opcert_info.opcert);
    TRACE("Opcert parsing status: %d\n", status);
    if (status != PARSING_OK) {
        return io_send_sw(SW_OPCERT_PARSING_FAIL);
    }
    G_context.state = STATE_PARSED;
    const parsed_opcert_t* opcert = &G_context.opcert_info.opcert;

    // Check security policy
    security_policy_t policy = policyForSignOpCert(&opcert->poolColdKeyPath);
    TRACE("Security policy: %d\n", policy);
    if (policy == POLICY_DENY) {
        TRACE("Security policy DENY - rejecting operation");
        nbgl_useCaseStatus("Operational certificate denied", false, ui_menu_main);
        // TODO make sure the constants are defined properly
        return io_send_sw(ERR_REJECTED_BY_POLICY);
    }

    ui_display_opcert(policy);

    return 0;
}

void finalize_sign_opcert(bool confirmed) {
    if (!confirmed) {
        G_context.state = STATE_NONE;
        io_send_sw(SW_DENY);
        return;
    }

    // user confirmed
    G_context.state = STATE_APPROVED;

    // assemble the opcert bytestring and sign it
    const parsed_opcert_t* opcert = &G_context.opcert_info.opcert;
    uint8_t opCertBodyBuffer[OP_CERT_BODY_LENGTH] = {0};
    explicit_bzero(opCertBodyBuffer, SIZEOF(opCertBodyBuffer));
    {
        write_view_t opCertBodyBufferView =
            make_write_view(opCertBodyBuffer, opCertBodyBuffer + OP_CERT_BODY_LENGTH);

        view_appendBuffer(&opCertBodyBufferView,
                        (const uint8_t*) opcert->kesPublicKey,
                        KES_PUBLIC_KEY_LENGTH);
        {
            uint8_t chunk[8] = {0};
            write_u64_be(chunk, 0, opcert->issueCounter);
#ifdef FUZZING
            view_appendBuffer(&opCertBodyBufferView, chunk, 8);
#else
            view_appendBuffer(&opCertBodyBufferView, chunk, SIZEOF(chunk));
#endif
        }
        {
            uint8_t chunk[8] = {0};
            write_u64_be(chunk, 0, opcert->kesPeriod);
#ifdef FUZZING
            view_appendBuffer(&opCertBodyBufferView, chunk, 8);
#else
            view_appendBuffer(&opCertBodyBufferView, chunk, SIZEOF(chunk));
#endif
        }

        ASSERT(view_processedSize(&opCertBodyBufferView) == OP_CERT_BODY_LENGTH);
        TRACE_BUFFER(opCertBodyBuffer, SIZEOF(opCertBodyBuffer));
    }

    ASSERT(bip44_isPoolColdKeyPath(&opcert->poolColdKeyPath));
    int r = signRawMessageWithPath(
        &opcert->poolColdKeyPath,
        opCertBodyBuffer, SIZEOF(opCertBodyBuffer),
        G_context.opcert_info.signature, SIZEOF(G_context.opcert_info.signature)
    );

    if (r != 0) {
        G_context.state = STATE_NONE;
        io_send_sw(SW_SIGNATURE_FAIL);
    } else {
        io_send_response_pointer(
            G_context.opcert_info.signature,
            SIZEOF(G_context.opcert_info.signature),
            SW_OK
        );
    }
}
