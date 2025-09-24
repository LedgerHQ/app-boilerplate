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

#include "parse_opcert.h"
#include "utils/utils.h"
#include "opcert_types.h"
#include "assert.h"


// TODO: This is a general buffer utility and should be moved to an appropriate file
// destBuffer is set to point at the beginning of the bytestring in buf;
// buf position is moved by len bytes
// if not enough bytes in the buffer, return false
bool buffer_read_bytes_ptr(buffer_t *buf, uint8_t** destBuffer, size_t len) {
    LEDGER_ASSERT(buf != NULL, "NULL buf");

    *destBuffer = (uint8_t *) (buf->ptr + buf->offset);
    if (!buffer_seek_cur(buf, len)) {
        return false;
    }
    return true;
}

opcert_parser_status_e opcert_deserialize(buffer_t *buf, parsed_opcert_t *opcert)
{
    LEDGER_ASSERT(buf != NULL, "NULL buf");
    LEDGER_ASSERT(opcert != NULL, "NULL tx");

    // KES public key
    if (!buffer_read_bytes_ptr(buf, &opcert->kesPublicKey, KES_PUBLIC_KEY_LENGTH)) {
        return KES_PUBLIC_KEY_PARSING_ERROR;
    }

    // KES period
    if (!buffer_read_u64(buf, &opcert->kesPeriod, BE)) {
        return KES_PERIOD_PARSING_ERROR;
    }

    // issue counter
    if (!buffer_read_u64(buf, &opcert->issueCounter, BE)) {
        return ISSUE_COUNTER_PARSING_ERROR;
    }

    // pool cold key path
    if (!buffer_read_bip44_path(buf, &opcert->poolColdKeyPath)) {
        return POOL_COLD_KEY_PATH_PARSING_ERROR;
    }

    return (buf->offset == buf->size) ? PARSING_OK : WRONG_LENGTH_ERROR;
}
