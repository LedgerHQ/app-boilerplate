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
#include <string.h>   // memset

#include "os.h"
#include "cx.h"

#include "get_public_key.h"
#include "../globals.h"
#include "../types.h"
#include "../io.h"
#include "../sw.h"
#include "../context.h"
#include "../common/buffer.h"
#include "../common/bip32.h"
#include "../ui/display.h"
#include "../helper/send_response.h"

int handler_get_public_key(buffer_t *cdata, bool display) {
    uint8_t raw_private_key[32] = {0};
    cx_ecfp_private_key_t private_key = {0};

    if (!buffer_read_u8(cdata, (uint8_t *) &pk_ctx.bip32_path_len) ||
        !bip32_path_from_buffer(cdata, pk_ctx.bip32_path, pk_ctx.bip32_path_len)) {
        return io_send_sw(SW_WRONG_DATA_LENGTH);
    }

    char str[30];
    bip32_path_to_str(pk_ctx.bip32_path, pk_ctx.bip32_path_len, str, sizeof(str));
    PRINTF("BIP32 path (%u): %s\n", pk_ctx.bip32_path_len, str);

    // derive the seed with bip32_path
    os_perso_derive_node_bip32(CX_CURVE_256K1,
                               pk_ctx.bip32_path,
                               pk_ctx.bip32_path_len,
                               raw_private_key,
                               pk_ctx.chain_code);
    // new private_key from raw
    cx_ecfp_init_private_key(CX_CURVE_256K1,
                             raw_private_key,
                             sizeof(raw_private_key),
                             &private_key);
    // generate corresponding public key
    cx_ecfp_generate_pair(CX_CURVE_256K1, &pk_ctx.public_key, &private_key, 1);

    // reset private keys
    memset(&private_key, 0, sizeof(private_key));
    memset(raw_private_key, 0, sizeof(raw_private_key));

    if (display) {
        ui_display_public_key();
        return 0;
    }

    return helper_send_response_pubkey(&pk_ctx);
}
