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

#include <stdint.h>
#include <stdbool.h>

#include "buffer.h"
#include "io.h"
#include "ledger_assert.h"

#include "dispatcher.h"
#include "constants.h"
#include "globals.h"
#include "types.h"
#include "sw.h"
#include "get_version.h"
#include "get_app_name.h"
#include "get_public_key.h"
#include "sign_tx.h"
#include "sign_opcert.h"

int apdu_dispatcher(const command_t *cmd) {
    LEDGER_ASSERT(cmd != NULL, "NULL cmd");
    TRACE("G_context.req_type: %d", G_context.req_type);
    TRACE("G_context.state: %d", G_context.state);

    if (cmd->cla != CLA) {
        return io_send_sw(SW_CLA_NOT_SUPPORTED);
    }

    buffer_t buf = {0};

    switch (cmd->ins) {
        case INS_GET_VERSION:
            if (cmd->p1 != P1_UNUSED || cmd->p2 != P2_UNUSED) {
                return io_send_sw(SW_WRONG_P1P2);
            }

            return handler_get_version();

        case INS_GET_APP_NAME:
            if (cmd->p1 != P1_UNUSED || cmd->p2 != P2_UNUSED) {
                return io_send_sw(SW_WRONG_P1P2);
            }

            return handler_get_app_name();

        case INS_GET_PUBLIC_KEY:
            if (cmd->p1 != P1_UNUSED || cmd->p2 != P2_UNUSED) {
                return io_send_sw(SW_WRONG_P1P2);
            }

            buf.ptr = cmd->data;
            buf.size = cmd->lc;
            buf.offset = 0;

            return handler_get_public_key(&buf);

        case INS_SIGN_TX:
            // Check if this is a witness APDU (P1 = 0x0f)
            if (cmd->p1 == 0x0f) {
                // Witness signing - P2 must be unused
                if (cmd->p2 != P2_UNUSED) {
                    return io_send_sw(SW_WRONG_P1P2);
                }

                if (!cmd->data) {
                    return io_send_sw(SW_WRONG_DATA_LENGTH);
                }

                buf.ptr = cmd->data;
                buf.size = cmd->lc;
                buf.offset = 0;

                return handler_sign_tx_witness(&buf);
            }

            // Transaction signing with new protocol:
            // P1 = P1_TX_INIT (0xFF) for INIT, P1_TX_DATA_CHUNK (0x01) for data chunks
            // P2 = P2_MORE (0x80) or P2_LAST (0x00)

            if (cmd->p1 == P1_TX_INIT) {
                // INIT APDU - P2 must be MORE (we always expect data after INIT)
                if (cmd->p2 != P2_MORE) {
                    return io_send_sw(SW_WRONG_P1P2);
                }
            } else if (cmd->p1 == P1_TX_DATA_CHUNK) {
                // Data chunk - P2 must be LAST or MORE
                if (cmd->p2 != P2_LAST && cmd->p2 != P2_MORE) {
                    return io_send_sw(SW_WRONG_P1P2);
                }
            } else {
                // Invalid P1 value
                return io_send_sw(SW_WRONG_P1P2);
            }

            if (!cmd->data) {
                return io_send_sw(SW_WRONG_DATA_LENGTH);
            }

            buf.ptr = cmd->data;
            buf.size = cmd->lc;
            buf.offset = 0;

            return handler_sign_tx(&buf, cmd->p1, (bool) (cmd->p2 & P2_MORE));

        case INS_SIGN_OPCERT:
            if (cmd->p1 != P1_UNUSED || cmd->p2 != P2_UNUSED) {
                return io_send_sw(SW_WRONG_P1P2);
            }
            buf.ptr = cmd->data;
            buf.size = cmd->lc;
            buf.offset = 0;

            return handler_sign_opcert(&buf);

        default:
            return io_send_sw(SW_INS_NOT_SUPPORTED);
    }
}
