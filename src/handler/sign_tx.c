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

#include "sign_tx.h"
#include "sw.h"
#include "globals.h"
#include "display.h"
#include "tx_types.h"
#include "deserialize.h"
#include "handle_swap.h"
#include "validate.h"
#include "token_db.h"
#include "swap_error_code_helpers.h"
#include "dynamic_token_info.h"

// This is a smart documentation inclusion. The full documentation is available at
// https://ledgerhq.github.io/app-exchange/
// --8<-- [start:ui_bypass]
#ifdef HAVE_SWAP
static int check_and_sign_swap_tx(transaction_ctx_t *tx_ctx) {
    if (G_swap_response_ready) {
        // Safety against trying to make the app sign multiple TX
        // This code should never be triggered as the app is supposed to exit after
        // sending the signed transaction
        PRINTF("Safety against double signing triggered\n");
        os_sched_exit(-1);
    } else {
        // We will quit the app after this transaction, whether it succeeds or fails
        PRINTF("Swap response is ready, the app will quit after the next send\n");
        // This boolean will make the io_send_sw family instant reply + return to exchange
        G_swap_response_ready = true;
    }
    if (swap_check_validity(tx_ctx->transaction.value,
                            tx_ctx->transaction.fee,
                            tx_ctx->transaction.to,
                            &tx_ctx->token_info)) {
        PRINTF("Swap response validated, sign the transaction\n");
        validate_transaction(true);
    }
    // Unreachable because swap_check_validity() returns an error to exchange app OR
    // validate_transaction() returns a success to exchange
    os_sched_exit(0);
    return 0;
}
#endif  // HAVE_SWAP
// --8<-- [end:ui_bypass]

/**
 * Initialize transaction context for chunk 0
 *
 * @param[in] cdata Buffer containing BIP32 path
 * @param[in] req_type Request type (CONFIRM_TRANSACTION or CONFIRM_TOKEN_TRANSACTION)
 * @return SWO_SUCCESS on success, error code otherwise
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
 * Accumulate transaction data from APDU chunks
 * TODO: This should NOT be handled in each handler but at the dispatcher level
 *
 * @param[in] cdata Buffer containing transaction chunk
 * @param[in] req_type Expected request type for validation
 * @return SWO_SUCCESS on success, error code otherwise
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

static uint16_t process_transaction(bool is_token_tx) {
    // last APDU for this transaction, let's parse, display and request a sign confirmation
    buffer_t buf = {.ptr = G_context.tx_info.raw_tx,
                    .size = G_context.tx_info.raw_tx_len,
                    .offset = 0};

    G_context.tx_info.is_token_tx = is_token_tx;
    parser_status_e status =
        transaction_deserialize(&buf, &G_context.tx_info.transaction, is_token_tx);
    PRINTF("Parsing status: %d.\n", status);
    if (status != PARSING_OK) {
        return SWO_INCORRECT_DATA;
    }

    if (is_token_tx) {
        // Look up token information from database
        if (!get_token_info(G_context.tx_info.transaction.token_address,
                            &G_context.tx_info.token_info)) {
            PRINTF("Token not found in database\n");
            return SWO_INCORRECT_DATA;
        }

        PRINTF("Token found: %s (decimals: %d)\n",
               G_context.tx_info.token_info.ticker,
               G_context.tx_info.token_info.decimals);
    }

    G_context.state = STATE_PARSED;

    if (cx_keccak_256_hash(G_context.tx_info.raw_tx,
                           G_context.tx_info.raw_tx_len,
                           G_context.tx_info.m_hash) != CX_OK) {
        return SWO_INCORRECT_DATA;
    }
    PRINTF("Hash: %.*H\n", sizeof(G_context.tx_info.m_hash), G_context.tx_info.m_hash);
    return SWO_SUCCESS;
}

int handler_sign_tx(buffer_t *cdata, uint8_t chunk, bool more, bool is_token_tx) {
    uint8_t req_type = is_token_tx ? CONFIRM_TOKEN_TRANSACTION : CONFIRM_TRANSACTION;
    if (chunk == 0) {
        // first APDU, parse BIP32 path and return
        return init_transaction_context(cdata, req_type);
    } else {
        // parse transaction
        uint16_t err = accumulate_transaction_data(cdata, req_type);
        if (err != SWO_SUCCESS) {
            return io_send_sw(err);
        }
        if (more) {
            // more APDUs with transaction part are expected.
            // Send a SWO_SUCCESS to signal that we have received the chunk
            return io_send_sw(SWO_SUCCESS);

        } else {
            // last APDU for this transaction, let's parse, display and request a sign confirmation
            err = process_transaction(is_token_tx);
            if (err != SWO_SUCCESS) {
#ifdef HAVE_SWAP
                if (G_called_from_swap) {
                    PRINTF("Error during transaction processing in swap context: %u\n", err);
                    // Suspicious error, Return to Exchange instead of simply return an error APDU
                    send_swap_error_simple(SW_SWAP_FAIL, SWAP_EC_ERROR_GENERIC, SWAP_ERROR_CODE);
                } else {
                    return io_send_sw(err);
                }
#else
                return io_send_sw(err);
#endif
            }

#ifdef HAVE_SWAP
            // If we are in swap context, do not redisplay the message data
            // Instead, ensure they are identical with what was previously displayed
            if (G_called_from_swap) {
                check_and_sign_swap_tx(&G_context.tx_info);
                // Unreachable
                return 0;
            }
#endif  // HAVE_SWAP

            // Example to trig a blind-sign flow
            if (strcmp((char *) G_context.tx_info.transaction.memo, "Blind-sign") == 0) {
                return ui_display_blind_signed_transaction();
            } else if (is_token_tx) {
                return ui_display_token_transaction();
            } else {
                return ui_display_transaction();
            }
        }
    }
    return 0;
}
