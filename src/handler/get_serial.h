#pragma once

/**
 * Handler for INS_GET_SERIAL command. Send APDU response with the device
 * serial number as provided by the BOLOS system call os_serial().
 *
 * @return zero or positive integer if success, negative integer otherwise.
 */
void handler_get_serial(void);
