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
#include "mem.h"

#include "deserialize.h"
#include "utils.h"
#include "types.h"
#include "tx_output_types.h"

#if defined(TEST) || defined(FUZZ)
#include "assert.h"
#define LEDGER_ASSERT(x, y) assert(x)
#else
#include "ledger_assert.h"
#endif

parser_status_e transaction_deserialize(buffer_t *buf, transaction_t *tx) {
    LEDGER_ASSERT(buf != NULL, "NULL buf");
    LEDGER_ASSERT(tx != NULL, "NULL tx");

    if (buf->size > MAX_TX_LEN) {
        return WRONG_LENGTH_ERROR;
    }

    // Initialize inputs list
    tx->inputs = NULL;

    // Note: num_inputs and num_outputs are already set from INIT APDU, not read from buffer

    // Parse each input and add to linked list
    for (uint16_t i = 0; i < tx->num_inputs; i++) {
        // Allocate memory for the input list item
        tx_input_list_item_t *item = (tx_input_list_item_t *) app_mem_alloc(sizeof(tx_input_list_item_t));
        if (item == NULL) {
            return INPUTS_PARSING_ERROR;
        }

        // Get pointer to transaction hash (32 bytes) and copy it
        uint8_t *txHash = (uint8_t *) (buf->ptr + buf->offset);
        if (!buffer_seek_cur(buf, TX_HASH_LENGTH)) {
            return INPUTS_PARSING_ERROR;
        }
        memmove(item->input_data.txHashBuffer, txHash, TX_HASH_LENGTH);

        // Read output index (uint32, big-endian)
        if (!buffer_read_u32(buf, &item->input_data.index, BE)) {
            return INPUTS_PARSING_ERROR;
        }

        // Add to linked list
        item->node.next = NULL;
        flist_push_back(&tx->inputs, (s_flist_node *) item);
    }

    // Initialize outputs list
    tx->outputs = NULL;

    // Parse each output and add to linked list
    for (uint16_t i = 0; i < tx->num_outputs; i++) {
        // Read output length (uint16, big-endian)
        uint16_t output_len;
        if (!buffer_read_u16(buf, &output_len, BE)) {
            return OUTPUTS_PARSING_ERROR;
        }

        // Track starting position for this output
        size_t output_start = buf->offset;

        // Allocate memory for the output list item
        tx_output_list_item_t *item = (tx_output_list_item_t *) app_mem_alloc(sizeof(tx_output_list_item_t));
        if (item == NULL) {
            return OUTPUTS_PARSING_ERROR;
        }

        // Read destination type (uint8)
        uint8_t dest_type;
        if (!buffer_read_u8(buf, &dest_type)) {
            return OUTPUTS_PARSING_ERROR;
        }
        item->output_data.destination.type = (tx_output_destination_type_t) dest_type;

        if (dest_type == DESTINATION_THIRD_PARTY) {
            // Read address size (uint16, big-endian)
            uint16_t addr_size;
            if (!buffer_read_u16(buf, &addr_size, BE)) {
                return OUTPUTS_PARSING_ERROR;
            }

            // Validate address size
            if (addr_size == 0 || addr_size > MAX_ADDRESS_SIZE) {
                return OUTPUT_ADDRESS_SIZE_ERROR;
            }
            item->output_data.destination.address.size = addr_size;

            // Read address bytes
            uint8_t *addr = (uint8_t *) (buf->ptr + buf->offset);
            if (!buffer_seek_cur(buf, addr_size)) {
                return OUTPUTS_PARSING_ERROR;
            }
            memmove(item->output_data.destination.address.buffer, addr, addr_size);

        } else if (dest_type == DESTINATION_DEVICE_OWNED) {
            // Read BIP44 path length (uint8)
            uint8_t path_len;
            if (!buffer_read_u8(buf, &path_len) || path_len > MAX_BIP32_PATH) {
                return OUTPUTS_PARSING_ERROR;
            }

            // Read BIP44 path
            if (!buffer_read_bip32_path(buf,
                                       item->output_data.destination.params.paymentKeyPath.path,
                                       path_len)) {
                return OUTPUTS_PARSING_ERROR;
            }
            item->output_data.destination.params.paymentKeyPath.length = path_len;

            // For now, set simple address type (BASE_PAYMENT_KEY_STAKE_KEY)
            // with same path for staking (typical change address)
            item->output_data.destination.params.type = BASE_PAYMENT_KEY_STAKE_KEY;
            item->output_data.destination.params.stakingDataSource = STAKING_KEY_PATH;
            memmove(&item->output_data.destination.params.stakingKeyPath,
                   &item->output_data.destination.params.paymentKeyPath,
                   sizeof(bip44_path_t));

            // Set network params (will be filled from transaction context)
            item->output_data.destination.params.networkId = 0;  // Will be set later
            item->output_data.destination.params.protocolMagic = 0;

        } else {
            return OUTPUT_DESTINATION_TYPE_ERROR;
        }

        // Read ADA amount (uint64, big-endian)
        if (!buffer_read_u64(buf, &item->output_data.adaAmount, BE)) {
            return OUTPUTS_PARSING_ERROR;
        }

        // Verify we consumed exactly output_len bytes
        if (buf->offset - output_start != output_len) {
            return OUTPUTS_PARSING_ERROR;
        }

        // Add to linked list
        item->node.next = NULL;
        flist_push_back(&tx->outputs, (s_flist_node *) item);
    }

    // fee value
    if (!buffer_read_u64(buf, &tx->fee, BE)) {
        return FEE_PARSING_ERROR;
    }

    // TTL value (optional, only if includeTtl is true)
    if (tx->includeTtl) {
        if (!buffer_read_u64(buf, &tx->ttl, BE)) {
            return TO_PARSING_ERROR;  // Reuse TO_PARSING_ERROR for TTL
        }
    }

    return (buf->offset == buf->size) ? PARSING_OK : WRONG_LENGTH_ERROR;
}
