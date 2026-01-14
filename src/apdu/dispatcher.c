/*****************************************************************************
 *   Ledger App Cardano.
 *   (c) 2025 Ledger SAS and Vacuumlabs
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

#include "parser.h"
#include "dispatcher.h"
#include "globals.h"
#include "apdu/apdu_constants.h"
#include "cardano_swo.h"
#include "utils/assert.h"
#include "utils/utils.h"
#include "app_context.h"
#include "get_serial.h"
#include "get_version.h"
#include "get_app_name.h"
#include "get_public_key.h"
#include "sign_tx.h"
#include "sign_opcert.h"
#ifdef DEBUG
#include "debug_settings.h"
#endif

/**
 * Map request type to its expected instruction
 * Used to detect instruction interleaving attacks
 */
static command_e req_type_to_instruction(request_type_e req_type) {
    switch (req_type) {
        case REQUEST_NONE:
            return INS_GET_VERSION;  // Dummy value, should never be checked
        case REQUEST_EXPORT_PUBKEY:
            return INS_GET_PUBLIC_KEY;
        case REQUEST_SIGN_TRANSACTION:
            return INS_SIGN_TX;
        case REQUEST_SIGN_OPCERT:
            return INS_SIGN_OPCERT;
        default:
            LEDGER_ASSERT(false, "Unknown request type");
            return INS_GET_VERSION;  // Unreachable
    }
}

void apdu_dispatcher(const command_t *cmd) {
    LEDGER_ASSERT(cmd != NULL, "NULL cmd");
    TRACE("G_context.req_type: %d", G_context.req_type);

    // Log the appropriate state based on request type
    switch (G_context.req_type) {
        case REQUEST_SIGN_TRANSACTION:
            TRACE("G_context.state.tx_state: %d", G_context.state.tx_state);
            break;
        case REQUEST_SIGN_OPCERT:
            TRACE("G_context.state.opcert_state: %d", G_context.state.opcert_state);
            break;
        default:
            // For stateless operations (GET_PUBLIC_KEY, GET_VERSION, etc.)
            break;
    }

    // Guard against instruction interleaving attacks
    // If an operation is in progress, only allow the same instruction to continue
    if (G_context.req_type != REQUEST_NONE) {
        command_e expected_ins = req_type_to_instruction(G_context.req_type);
        if (cmd->ins != expected_ins) {
            TRACE("Instruction interleaving detected: current=%d (req_type=%d), attempted=%d",
                  expected_ins, G_context.req_type, cmd->ins);
            send_swo_and_reset(SWO_COMMAND_NOT_ALLOWED);
            return;
        }
        TRACE("Same instruction continuing: ins=%d", cmd->ins);
    } else {
        // This is a new request, ensure we start with a clean context
        reset_app_context();
    }

    if (cmd->cla != CLA) {
        send_swo_and_reset(SWO_INVALID_CLA);
        return;
    }

    switch (cmd->ins) {
        case INS_GET_SERIAL:
            if (cmd->p1 != P1_UNUSED || cmd->p2 != P2_UNUSED) {
                send_swo_and_reset(SWO_INCORRECT_P1_P2);
                return;
            }

            handler_get_serial();
            return;

        case INS_GET_VERSION:
            if (cmd->p1 != P1_UNUSED || cmd->p2 != P2_UNUSED) {
                send_swo_and_reset(SWO_INCORRECT_P1_P2);
                return;
            }

            handler_get_version();
            return;

        case INS_GET_APP_NAME:
            if (cmd->p1 != P1_UNUSED || cmd->p2 != P2_UNUSED) {
                send_swo_and_reset(SWO_INCORRECT_P1_P2);
                return;
            }

            handler_get_app_name();
            return;

        case INS_GET_PUBLIC_KEY: {
            if (cmd->p1 != P1_UNUSED || cmd->p2 != P2_UNUSED) {
                send_swo_and_reset(SWO_INCORRECT_P1_P2);
                return;
            }

            buffer_t pubkey_buf = {0};
            pubkey_buf.ptr = cmd->data;
            pubkey_buf.size = cmd->lc;
            pubkey_buf.offset = 0;

            handler_get_public_key(&pubkey_buf);
            return;
        }

        case INS_SIGN_TX:
            // Check if this is a witness APDU (P1 = 0x0f)
            if (cmd->p1 == 0x0f) {
                // Witness signing - P2 must be unused
                if (cmd->p2 != P2_UNUSED) {
                    send_swo_and_reset(SWO_INCORRECT_P1_P2);
                    return;
                }

                if (!cmd->data) {
                    send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
                    return;
                }

                buffer_t witness_buf = {0};
                witness_buf.ptr = cmd->data;
                witness_buf.size = cmd->lc;
                witness_buf.offset = 0;

                handler_sign_tx_witness(&witness_buf);
                return;
            }

            // Transaction signing with redesigned protocol:
            // P1 controls flow: P1_TX_INIT (0x00), P1_TX_DATA_CHUNK (0x01),
            // P1_TX_CHUNK_LAST (0x02), P1_TX_AUX_DATA (0x03)
            if (cmd->p1 == P1_TX_AUX_DATA) {
                if (cmd->p2 != P2_AUX_DATA_INIT && cmd->p2 != P2_AUX_DATA_DELEGATION) {
                    send_swo_and_reset(SWO_INCORRECT_P1_P2);
                    return;
                }
            } else {
                // P2 must be unused for non-AUX_DATA APDUs
                if (cmd->p2 != P2_UNUSED) {
                    send_swo_and_reset(SWO_INCORRECT_P1_P2);
                    return;
                }
            }

            // Validate P1 value
            if (cmd->p1 != P1_TX_INIT &&
                cmd->p1 != P1_TX_DATA_CHUNK &&
                cmd->p1 != P1_TX_CHUNK_LAST &&
                cmd->p1 != P1_TX_AUX_DATA) {
                send_swo_and_reset(SWO_INCORRECT_P1_P2);
                return;
            }

            if (!cmd->data) {
                send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
                return;
            }

            buffer_t tx_buf = {0};
            tx_buf.ptr = cmd->data;
            tx_buf.size = cmd->lc;
            tx_buf.offset = 0;

            if (cmd->p1 == P1_TX_AUX_DATA) {
                handler_sign_tx_aux_data(&tx_buf, cmd->p2);
                return;
            }

            // Determine if more data follows based on P1
            // P1_TX_CHUNK_LAST signals no more data, all others signal more data to come
            bool more = (cmd->p1 != P1_TX_CHUNK_LAST);
            handler_sign_tx(&tx_buf, cmd->p1, more);
            return;

        case INS_SIGN_OPCERT: {
            if (cmd->p1 != P1_UNUSED || cmd->p2 != P2_UNUSED) {
                send_swo_and_reset(SWO_INCORRECT_P1_P2);
                return;
            }
            buffer_t opcert_buf = {0};
            opcert_buf.ptr = cmd->data;
            opcert_buf.size = cmd->lc;
            opcert_buf.offset = 0;

            handler_sign_opcert(&opcert_buf);
            return;
        }

#ifdef DEBUG
        case INS_DEBUG_SET_SETTINGS: {
            // Debug-only command to set app settings for testing
            if (cmd->p1 != P1_UNUSED || cmd->p2 != P2_UNUSED) {
                send_swo_and_reset(SWO_INCORRECT_P1_P2);
                return;
            }

            buffer_t debug_buf = {0};
            debug_buf.ptr = cmd->data;
            debug_buf.size = cmd->lc;
            debug_buf.offset = 0;

            handler_debug_set_settings(&debug_buf);
            return;
        }
#endif

        default:
            send_swo_and_reset(SWO_INVALID_INS);
            return;
    }
}
