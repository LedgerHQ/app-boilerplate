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
#include "bip32.h"

#include "get_public_key.h"
#include "sw.h"
#include "ui/display.h"

static void send_response_pubkey(const uint8_t raw_public_key[static PUBKEY_LEN], const uint8_t chain_code[static CHAINCODE_LEN]) {
    uint8_t pubkey_len[1] = {PUBKEY_LEN};
    uint8_t chain_code_len[1] = {CHAINCODE_LEN};

    buffer_t buffers[4] = {
        {.ptr = pubkey_len, .size = sizeof(pubkey_len), .offset = 0},
        {.ptr = raw_public_key, .size = PUBKEY_LEN, .offset = 0},
        {.ptr = chain_code_len, .size = sizeof(chain_code_len), .offset = 0},
        {.ptr = chain_code, .size = CHAINCODE_LEN, .offset = 0},
    };

    io_send_response_buffers(buffers, 4, SW_OK);
}

int handler_get_public_key(buffer_t *cdata, bool display) {
    uint8_t bip32_path_len;
    uint32_t bip32_path[MAX_BIP32_PATH] = {0};
    if (!buffer_read_u8(cdata, &bip32_path_len) ||
        !buffer_read_bip32_path(cdata, bip32_path, (size_t) bip32_path_len)) {
        return io_send_sw(SW_WRONG_DATA_LENGTH);
    }


    uint8_t raw_public_key[PUBKEY_LEN] = {0};
    uint8_t chain_code[CHAINCODE_LEN] = {0};
    cx_err_t error = bip32_derive_get_pubkey_256(CX_CURVE_256K1,
                                                 bip32_path,
                                                 bip32_path_len,
                                                 raw_public_key,
                                                 chain_code,
                                                 CX_SHA512);

    if (error != CX_OK) {
        return io_send_sw(error);
    }

    ui_ret_e ret = UI_RET_APPROVED;
    if (display) {
        ret = ui_display_address(raw_public_key);
    }

    if (ret == UI_RET_APPROVED) {
        send_response_pubkey(raw_public_key, chain_code);
    } else if (ret == UI_RET_REJECTED) {
        io_send_sw(SW_DENY);
    } else {
        io_send_sw(SW_DISPLAY_ADDRESS_FAIL);
    }


    if (display) {
        ui_display_address_status(ret);
    }

    return 0;
}
