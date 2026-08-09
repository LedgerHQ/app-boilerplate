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
#include <string.h>

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
#include "provide_token_info.h"

/**
 * Initialize transaction context for the first APDU chunk (P1_START).
 * Clears global context, sets request type, and parses BIP32 path.
 *
 * @param[in] cdata    Buffer containing BIP32 path
 * @param[in] req_type Request type (CONFIRM_TRANSACTION or CONFIRM_TOKEN_TRANSACTION)
 *
 * @return zero or positive integer if success, negative integer otherwise.
 */
static int init_transaction_context(buffer_t *cdata, uint8_t req_type) {
    explicit_bzero(&G_context, sizeof(G_context));
    G_context.req_type = req_type;
    G_context.state = STATE_NONE;

    if (!buffer_read_u8(cdata, &G_context.bip32_path_len) ||
        !buffer_read_bip32_path(cdata, G_context.bip32_path, (size_t) G_context.bip32_path_len)) {
        return io_send_sw(SWO_WRONG_DATA_LENGTH);
    }

    return io_send_sw(SWO_SUCCESS);
}

/**
 * Accumulate transaction data from an APDU chunk into the global raw_tx buffer.
 *
 * @param[in] cdata    Buffer containing transaction data chunk
 * @param[in] req_type Expected request type for validation
 *
 * @return SWO_SUCCESS on success, error status word otherwise.
 */
static uint16_t accumulate_transaction_data(buffer_t *cdata, uint8_t req_type) {
    if (G_context.req_type != req_type) {
        return SWO_CONDITIONS_NOT_SATISFIED;
    }
    if (G_context.tx_info.raw_tx_len + cdata->size > sizeof(G_context.tx_info.raw_tx)) {
        return SWO_WRONG_DATA_LENGTH;
    }
    if (!buffer_move(cdata, G_context.tx_info.raw_tx + G_context.tx_info.raw_tx_len, cdata->size)) {
        return SWO_INCORRECT_DATA;
    }
    G_context.tx_info.raw_tx_len += cdata->size;
    return SWO_SUCCESS;
}

int apdu_dispatcher(const command_t *cmd) {
    LEDGER_ASSERT(cmd != NULL, "NULL cmd");

    if (cmd->cla != CLA) {
        return io_send_sw(SWO_INVALID_CLA);
    }

    buffer_t buf = {0};

    switch (cmd->ins) {
        case GET_VERSION:
            if (cmd->p1 != 0 || cmd->p2 != 0) {
                return io_send_sw(SWO_INCORRECT_P1_P2);
            }

            return handler_get_version();

        case GET_APP_NAME:
            if (cmd->p1 != 0 || cmd->p2 != 0) {
                return io_send_sw(SWO_INCORRECT_P1_P2);
            }

            return handler_get_app_name();

        case GET_PUBLIC_KEY:
            if (cmd->p1 > 1 || cmd->p2 > 0) {
                return io_send_sw(SWO_INCORRECT_P1_P2);
            }

            if (!cmd->data) {
                return io_send_sw(SWO_WRONG_DATA_LENGTH);
            }

            buf.ptr = cmd->data;
            buf.size = cmd->lc;
            buf.offset = 0;

            return handler_get_public_key(&buf, (bool) cmd->p1);

        case SIGN_TX:
        case SIGN_TOKEN_TX: {
            // Common handler for both SIGN_TX and SIGN_TOKEN_TX, the content is very similar
            PRINTF("APDU_DISPATCHER: %d\n", cmd->ins);
            if ((cmd->p1 == P1_START && cmd->p2 != P2_MORE) ||  //
                cmd->p1 > P1_MAX ||                             //
                (cmd->p2 != P2_LAST && cmd->p2 != P2_MORE)) {
                return io_send_sw(SWO_INCORRECT_P1_P2);
            }

            if (!cmd->data) {
                return io_send_sw(SWO_WRONG_DATA_LENGTH);
            }

            buf.ptr = cmd->data;
            buf.size = cmd->lc;
            buf.offset = 0;

            bool is_token_tx = (cmd->ins == SIGN_TOKEN_TX);
            uint8_t req_type = is_token_tx ? CONFIRM_TOKEN_TRANSACTION : CONFIRM_TRANSACTION;

            if (cmd->p1 == P1_START) {
                // First APDU chunk: initialize context and parse BIP32 path
                return init_transaction_context(&buf, req_type);
            }

            // Subsequent chunks: accumulate transaction data
            uint16_t err = accumulate_transaction_data(&buf, req_type);
            if (err != SWO_SUCCESS) {
                return io_send_sw(err);
            }

            if (cmd->p2 & P2_MORE) {
                // More chunks expected, acknowledge reception
                return io_send_sw(SWO_SUCCESS);
            }

            // Last chunk received: all transaction data is reassembled, call the handler
            return handler_sign_tx(is_token_tx);
        }

        case PROVIDE_TOKEN_INFO:
            if (cmd->p1 != 0 || cmd->p2 != 0) {
                return io_send_sw(SWO_INCORRECT_P1_P2);
            }

            if (!cmd->data) {
                return io_send_sw(SWO_WRONG_DATA_LENGTH);
            }

            buf.ptr = cmd->data;
            buf.size = cmd->lc;
            buf.offset = 0;

            return handler_provide_token_info(&buf);

        default:
            return io_send_sw(SWO_INVALID_INS);
    }
}
