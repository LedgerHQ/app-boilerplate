#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "buffer.h"
#include "tx_output_types.h"
#include "tx_parse.h"

/**
 * Parse transaction output destination from buffer.
 *
 * Parses either a third-party address (raw bytes) or device-owned address (params).
 * This is a common operation when parsing transaction outputs.
 *
 * Wire format:
 * - destination type: 1 byte (DESTINATION_THIRD_PARTY=1 or DESTINATION_DEVICE_OWNED=2)
 * - if DESTINATION_THIRD_PARTY:
 *     - address size: 2 bytes (BE)
 *     - address bytes: <size> bytes
 * - if DESTINATION_DEVICE_OWNED:
 *     - address params (see buffer_parseAddressParams in addressUtilsShelley.h)
 *
 * @param[in,out] buf Buffer to read from
 * @param[out] destination Destination structure to populate
 * @param[in] networkId Network ID (used for DEVICE_OWNED addresses)
 * @return PARSING_OK on success, appropriate error code on failure
 */
parser_status_e parse_output_destination(buffer_t* buf,
                                         tx_output_destination_storage_t* destination,
                                         uint8_t networkId);

/**
 * Parse transaction output serialization format from buffer.
 *
 * Validates that the format is either ARRAY_LEGACY or MAP_BABBAGE.
 *
 * Wire format:
 * - format: 1 byte (ARRAY_LEGACY=0 or MAP_BABBAGE=1)
 *
 * @param[in,out] buf Buffer to read from
 * @param[out] format Output format to populate
 * @return PARSING_OK on success, OUTPUTS_PARSING_ERROR on failure
 */
parser_status_e parse_output_format(buffer_t* buf,
                                    tx_output_serialization_format_t* format);

/**
 * Parse transaction output datum from buffer.
 *
 * Wire format:
 * - datum_wire_type: 1 byte (0=NONE, 1=HASH, 2=INLINE)
 * - if HASH (1):
 *     - hash bytes: 32 bytes (no length prefix)
 * - if INLINE (2):
 *     - size: 2 bytes (BE)
 *     - data bytes: <size> bytes
 *
 * @param[in,out] buf Buffer to read from
 * @param[out] datum Datum structure to populate
 * @return PARSING_OK on success, OUTPUTS_PARSING_ERROR on failure
 */
parser_status_e parse_output_datum(buffer_t* buf, output_datum_t* datum);

/**
 * Parse transaction output reference script from buffer.
 *
 * Wire format:
 * - has_ref_script: 1 byte (0=NONE, 2=HAS_SCRIPT)
 * - if HAS_SCRIPT (2):
 *     - size: 2 bytes (BE)
 *     - data bytes: <size> bytes
 *
 * @param[in,out] buf Buffer to read from
 * @param[out] refScript Reference script structure to populate
 * @param[out] hasRefScript Whether reference script is present
 * @return PARSING_OK on success, OUTPUTS_PARSING_ERROR on failure
 */
parser_status_e parse_output_ref_script(buffer_t* buf,
                                        ref_script_t* refScript,
                                        bool* hasRefScript);
