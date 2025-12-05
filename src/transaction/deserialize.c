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
#include "buffer.h"

#include "deserialize.h"
#include "utils.h"
#include "types.h"

#if defined(TEST) || defined(FUZZ)
#include "assert.h"
#define LEDGER_ASSERT(x, y) assert(x)
#else
#include "ledger_assert.h"
#endif

parser_status_e transaction_deserialize(buffer_t *buf,
                                        transaction_t *tx,
                                        bool is_token_transaction) {
    LEDGER_ASSERT(buf != NULL, "NULL buf");
    LEDGER_ASSERT(tx != NULL, "NULL tx");

    if (buf->size > MAX_TX_LEN) {
        PRINTF("WRONG_LENGTH_ERROR\n");
        return WRONG_LENGTH_ERROR;
    }

    // nonce
    if (!buffer_read_u64(buf, &tx->nonce, BE)) {
        PRINTF("NONCE_PARSING_ERROR\n");
        return NONCE_PARSING_ERROR;
    }

    tx->to = (uint8_t *) (buf->ptr + buf->offset);

    // TO address
    if (!buffer_seek_cur(buf, ADDRESS_LEN)) {
        PRINTF("TO_PARSING_ERROR\n");
        return TO_PARSING_ERROR;
    }

    if (is_token_transaction) {
        // token address (32 bytes)
        tx->token_address = (uint8_t *) (buf->ptr + buf->offset);
        if (!buffer_seek_cur(buf, TOKEN_ADDRESS_LEN)) {
            PRINTF("TOKEN_ADDRESS_PARSING_ERROR\n");
            return TOKEN_ADDRESS_PARSING_ERROR;
        }
    } else {
        tx->token_address = NULL;
    }

    // amount value
    if (!buffer_read_u64(buf, &tx->value, BE)) {
        PRINTF("VALUE_PARSING_ERROR\n");
        return VALUE_PARSING_ERROR;
    }

    // fee value
    tx->fee = 0;  // default fee value

    // length of memo
    if (!buffer_read_varint(buf, &tx->memo_len) && tx->memo_len > MAX_MEMO_LEN) {
        PRINTF("MEMO_LENGTH_ERROR\n");
        return MEMO_LENGTH_ERROR;
    }

    // memo
    tx->memo = (uint8_t *) (buf->ptr + buf->offset);

    if (!buffer_seek_cur(buf, tx->memo_len)) {
        PRINTF("MEMO_PARSING_ERROR\n");
        return MEMO_PARSING_ERROR;
    }

    if (!transaction_utils_check_encoding(tx->memo, tx->memo_len)) {
        PRINTF("MEMO_ENCODING_ERROR\n");
        return MEMO_ENCODING_ERROR;
    }

    return (buf->offset == buf->size) ? PARSING_OK : WRONG_LENGTH_ERROR;
}
