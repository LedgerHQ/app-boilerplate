/*****************************************************************************
 *   Ledger App Cardano.
 *   (c) 2025 Vacuumlabs
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

#include "tx_parse_outputs.h"
#include "utils/assert.h"
#include "utils/buffer_utils.h"
#include "addressUtils/addressUtilsShelley.h"
#include "cardano_swo.h"

parser_status_e parse_output_destination(buffer_t* buf,
                                         tx_output_destination_storage_t* destination,
                                         uint8_t networkId) {
    LEDGER_ASSERT(buf != NULL, "NULL buf");
    LEDGER_ASSERT(destination != NULL, "NULL destination");

    // Read destination type
    uint8_t dest_type = 0;
    if (!buffer_read_u8(buf, &dest_type)) {
        return OUTPUTS_PARSING_ERROR;
    }
    TRACE("Deserialize: Output destination type=0x%02x (1=THIRD_PARTY, 2=DEVICE_OWNED)", dest_type);
    destination->type = (tx_output_destination_type_t) dest_type;

    switch (dest_type) {
        case DESTINATION_THIRD_PARTY: {
            // Read address size
            uint16_t addr_size = 0;
            if (!buffer_read_u16(buf, &addr_size, BE)) {
                return OUTPUTS_PARSING_ERROR;
            }
            if (addr_size == 0 || addr_size > MAX_ADDRESS_LENGTH) {
                return OUTPUT_ADDRESS_SIZE_ERROR;
            }
            destination->address.size = addr_size;

            // Store pointer to address in raw buffer instead of copying
            uint8_t *addr_ptr = NULL;
            if (!buffer_read_bytes_ptr(buf, &addr_ptr, addr_size)) {
                return OUTPUTS_PARSING_ERROR;
            }
            ASSERT(addr_ptr != NULL);
            destination->address.buffer = addr_ptr;
            break;
        }

        case DESTINATION_DEVICE_OWNED: {
            // Parse address params
            // Wire format should include: address_type + [protocol_magic for Byron | network_id for Shelley] + address_data
            if (!buffer_parseAddressParams(buf, &destination->params)) {
                return OUTPUTS_PARSING_ERROR;
            }
            // Override network ID if needed (for Shelley addresses)
            // Byron addresses use protocol magic, not network ID
            if (destination->params.type != BYRON) {
                destination->params.networkId = networkId;
            }
            break;
        }

        default:
            return OUTPUT_DESTINATION_TYPE_ERROR;
    }

    return PARSING_OK;
}

parser_status_e parse_output_format(buffer_t* buf,
                                    tx_output_serialization_format_t* format) {
    LEDGER_ASSERT(buf != NULL, "NULL buf");
    LEDGER_ASSERT(format != NULL, "NULL format");

    uint8_t output_format = 0;
    if (!buffer_read_u8(buf, &output_format)) {
        return OUTPUTS_PARSING_ERROR;
    }

    // Validate format is one of the supported values
    if (output_format != ARRAY_LEGACY && output_format != MAP_BABBAGE) {
        TRACE("Invalid output format: %u", output_format);
        return OUTPUTS_PARSING_ERROR;
    }

    *format = (tx_output_serialization_format_t) output_format;
    TRACE("Output serialization format: %u", output_format);

    return PARSING_OK;
}

parser_status_e parse_output_datum(buffer_t* buf, output_datum_t* datum) {
    LEDGER_ASSERT(buf != NULL, "NULL buf");
    LEDGER_ASSERT(datum != NULL, "NULL datum");

    size_t offset_before = buf->offset;
    uint8_t datum_wire_type;
    if (!buffer_read_u8(buf, &datum_wire_type)) {
        return OUTPUTS_PARSING_ERROR;
    }
    TRACE("Datum: wire=%u, offset %u -> %u", datum_wire_type, (unsigned int)offset_before, (unsigned int)buf->offset);

    switch (datum_wire_type) {
        case 0:  // No datum
            datum->hasDatum = false;
            break;

        case 1: {  // Datum hash
            datum->hasDatum = true;
            datum->type = DATUM_HASH;

            uint8_t *hash_ptr = NULL;
            if (!buffer_read_bytes_ptr(buf, &hash_ptr, OUTPUT_DATUM_HASH_LENGTH)) {
                return OUTPUTS_PARSING_ERROR;
            }
            ASSERT(hash_ptr != NULL);
            datum->hash = hash_ptr;
            TRACE("Datum hash read");
            TRACE_BUFFER(datum->hash, OUTPUT_DATUM_HASH_LENGTH);
            break;
        }

        case 2: {  // Inline datum
            datum->hasDatum = true;
            datum->type = DATUM_INLINE;

            uint16_t datum_size;
            if (!buffer_read_u16(buf, &datum_size, BE)) {
                return OUTPUTS_PARSING_ERROR;
            }
            if (datum_size > MAX_DATUM_INLINE_LENGTH) {
                return OUTPUTS_PARSING_ERROR;
            }
            datum->inline_data.size = datum_size;

            uint8_t *data_ptr = NULL;
            if (!buffer_read_bytes_ptr(buf, &data_ptr, datum_size)) {
                return OUTPUTS_PARSING_ERROR;
            }
            ASSERT(data_ptr != NULL);
            datum->inline_data.data = data_ptr;
            TRACE("Inline datum read: %u bytes", datum_size);
            TRACE_BUFFER(datum->inline_data.data, datum->inline_data.size);
            break;
        }

        default:
            return OUTPUTS_PARSING_ERROR;
    }

    return PARSING_OK;
}

parser_status_e parse_output_ref_script(buffer_t* buf,
                                        ref_script_t* refScript) {
    LEDGER_ASSERT(buf != NULL, "NULL buf");
    LEDGER_ASSERT(refScript != NULL, "NULL refScript");

    size_t offset_before = buf->offset;
    uint8_t has_ref_script_wire;
    if (!buffer_read_u8(buf, &has_ref_script_wire)) {
        return OUTPUTS_PARSING_ERROR;
    }
    TRACE("Reference script: wire=%u, offset %u -> %u", has_ref_script_wire, (unsigned int)offset_before, (unsigned int)buf->offset);

    switch (has_ref_script_wire) {
        case 0:  // No reference script
            refScript->hasRefScript = false;
            refScript->size = 0;
            refScript->data = NULL;
            break;

        case 2: {  // Has reference script
            refScript->hasRefScript = true;

            uint16_t script_size;
            if (!buffer_read_u16(buf, &script_size, BE)) {
                return OUTPUTS_PARSING_ERROR;
            }
            if (script_size > MAX_REF_SCRIPT_LENGTH) {
                return OUTPUTS_PARSING_ERROR;
            }
            refScript->size = script_size;

            uint8_t *data_ptr = NULL;
            if (!buffer_read_bytes_ptr(buf, &data_ptr, script_size)) {
                return OUTPUTS_PARSING_ERROR;
            }
            ASSERT(data_ptr != NULL);
            refScript->data = data_ptr;
            TRACE("Reference script read: %u bytes", script_size);
            break;
        }

        default:
            return OUTPUTS_PARSING_ERROR;
    }

    return PARSING_OK;
}
