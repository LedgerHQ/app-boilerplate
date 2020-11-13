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

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t
#include <string.h>  // memmove

#include "send_response.h"
#include "../context.h"
#include "../sw.h"
#include "../globals.h"
#include "common/buffer.h"

int helper_send_response_pubkey(pubkey_ctx_t *pk_ctx) {
    uint8_t resp[1 + PUBKEY_LEN + 1 + CHAINCODE_LEN] = {0};
    size_t offset = 0;

    resp[offset++] = PUBKEY_LEN;
    memmove(resp + offset, pk_ctx->public_key.W, PUBKEY_LEN);
    offset += PUBKEY_LEN;
    resp[offset++] = CHAINCODE_LEN;
    memmove(resp + offset, pk_ctx->chain_code, CHAINCODE_LEN);
    offset += CHAINCODE_LEN;

    buffer_t rdata = {.ptr = resp, .size = offset, .offset = 0};

    return io_send_response(&rdata, SW_OK);
}
