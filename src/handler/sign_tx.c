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
#include "parser.h"
#include "bip32.h"
#include "io.h"
#include "crypto_helpers.h"

#include "sign_tx.h"
#include "sw.h"
#include "ui/display.h"
#include "transaction/types.h"
#include "transaction/deserialize.h"

/**
 * Parameter 2 for last APDU to receive.
 */
#define P2_LAST 0x00
/**
 * Parameter 2 for more APDU to receive.
 */
#define P2_MORE 0x80
/**
 * Parameter 1 for first APDU number.
 */
#define P1_START 0x00
/**
 * Parameter 1 for maximum APDU number.
 */
#define P1_MAX 0x03


static uint16_t get_next_chunk(buffer_t *cdata, bool *more) {
    int input_len = 0;
    command_t cmd;

    if ((input_len = io_recv_command()) < 0) {
        LEDGER_ASSERT(false, "=> io_recv_command failure\n");
    }

    // Parse APDU command from G_io_apdu_buffer
    if (!apdu_parser(&cmd, G_io_apdu_buffer, input_len)) {
        PRINTF("=> /!\\ BAD LENGTH: %.*H\n", input_len, G_io_apdu_buffer);
        return SW_WRONG_DATA_LENGTH;
    }

    PRINTF("=> CLA=%02X | INS=%02X | P1=%02X | P2=%02X | Lc=%02X | CData=%.*H\n",
           cmd.cla,
           cmd.ins,
           cmd.p1,
           cmd.p2,
           cmd.lc,
           cmd.lc,
           cmd.data);

    if (cmd.cla != CLA) {
        return SW_BAD_STATE;
    }

    if (cmd.ins != SIGN_TX) {
        return SW_BAD_STATE;
    }

    if (cmd.p1 == P1_START) {
        return SW_BAD_STATE;
    }

    if ((cmd.p2 != P2_MORE) && (cmd.p2 != P2_LAST)) {
        return SW_WRONG_P1P2;
    }

    if (!cmd.data) {
        return SW_WRONG_DATA_LENGTH;
    }

    cdata->ptr = cmd.data;
    cdata->size = cmd.lc;
    cdata->offset = 0;

    if (cmd.p2 == P2_MORE) {
        *more = true;
    } else {
        *more = false;
    }

    return SW_OK;
}

static cx_err_t crypto_sign_message(const uint32_t *bip32_path,
                                    uint8_t bip32_path_len,
                                    const uint8_t m_hash[static 32],
                                    uint8_t signature[static MAX_DER_SIG_LEN],
                                    uint8_t *signature_len,
                                    uint8_t *v) {
    uint32_t info = 0;
    size_t sig_len = MAX_DER_SIG_LEN;

    cx_err_t error = bip32_derive_ecdsa_sign_hash_256(CX_CURVE_256K1,
                                                      bip32_path,
                                                      bip32_path_len,
                                                      CX_RND_RFC6979 | CX_LAST,
                                                      CX_SHA256,
                                                      m_hash,
                                                      32,
                                                      signature,
                                                      &sig_len,
                                                      &info);
    if (error != CX_OK) {
        return error;
    }

    PRINTF("Signature: %.*H\n", sig_len, signature);

    *signature_len = sig_len;
    *v = (uint8_t)(info & CX_ECCINFO_PARITY_ODD);

    return CX_OK;
}


static void helper_send_response_sig(uint8_t signature_len, const uint8_t *signature, uint8_t v) {
    uint8_t _signature_len[1] = {signature_len};
    uint8_t _v[1] = {v};

    buffer_t buffers[3] = {
        {.ptr = _signature_len, .size = sizeof(_signature_len), .offset = 0},
        {.ptr = signature, .size = signature_len, .offset = 0},
        {.ptr = _v, .size = sizeof(_v), .offset = 0},
    };

    io_send_response_buffers(buffers, 3, SW_OK);
}



int handler_sign_tx(buffer_t *cdata, uint8_t p1, uint8_t p2) {
    if (p1 != P1_START) {
        return io_send_sw(SW_BAD_STATE);
    }

    if (p2 != P2_MORE) {
        return io_send_sw(SW_BAD_STATE);
    }

    uint8_t bip32_path_len;
    uint32_t bip32_path[MAX_BIP32_PATH] = {0};
    if (!buffer_read_u8(cdata, &bip32_path_len) ||
        !buffer_read_bip32_path(cdata, bip32_path, (size_t) bip32_path_len)) {
        return io_send_sw(SW_WRONG_DATA_LENGTH);
    }

    uint8_t raw_tx[MAX_TRANSACTION_LEN];  /// raw transaction serialized
    size_t raw_tx_len = 0;                /// length of raw transaction
    bool more = true;
    while (more) {
        io_send_sw(SW_OK);

        uint16_t sw = get_next_chunk(cdata, &more);

        if (sw != SW_OK) {
            return io_send_sw(sw);
        }

        if (raw_tx_len + cdata->size > sizeof(raw_tx)) {
            return io_send_sw(SW_WRONG_TX_LENGTH);
        }

        if (!buffer_move(cdata,
                         raw_tx + raw_tx_len,
                         cdata->size)) {
            return io_send_sw(SW_TX_PARSING_FAIL);
        }
        raw_tx_len += cdata->size;
    }

    // last APDU for this transaction, let's parse, display and request a sign confirmation

    buffer_t buf = {.ptr = raw_tx,
                    .size = raw_tx_len,
                    .offset = 0};
    transaction_t transaction = {0};

    parser_status_e status = transaction_deserialize(&buf, &transaction);
    PRINTF("Parsing status: %d.\n", status);
    if (status != PARSING_OK) {
        return io_send_sw(SW_TX_PARSING_FAIL);
    }

    uint8_t m_hash[32];
    cx_keccak_256_hash(raw_tx, raw_tx_len, m_hash);
    PRINTF("Hash: %.*H\n", 32, m_hash);

    ui_ret_e ret = ui_display_transaction(&transaction);

    if (ret == UI_RET_APPROVED) {
        uint8_t signature[MAX_DER_SIG_LEN];
        uint8_t signature_len;
        uint8_t v;
        cx_err_t err;

        err = crypto_sign_message(bip32_path,
                                  bip32_path_len,
                                  m_hash,
                                  signature,
                                  &signature_len,
                                  &v);
        if (err != CX_OK) {
            io_send_sw(SW_SIGNATURE_FAIL);
        }
        helper_send_response_sig(signature_len, signature, v);
    } else if (ret == UI_RET_REJECTED) {
        io_send_sw(SW_DENY);
    } else {
        io_send_sw(SW_BAD_STATE);
    }

    ui_display_transaction_status(ret);

    return 0;
}
