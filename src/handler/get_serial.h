#pragma once

#include "buffer.h"

//TODO: modify return in comment
/**
 * Handler for INS_GET_SERIAL command. Send APDU response with the device
 * serial number as provided by the BOLOS system call os_serial().
 *
 * @param[in] data_buffer
 *   Buffer containing APDU data (must be empty for this command)
 *
 * @return zero or positive integer if success, negative integer otherwise.
 */
void handler_get_serial(const buffer_t *data_buffer);
