#pragma once

#include "buffer.h"

#define APPVERSION_LEN 3

//TODO: modify return in comment
/**
 * Handler gor INS_GET_VERSION command. Send APDU response with version
 * of the application.
 *
 * @param[in] data_buffer
 *   Buffer containing APDU data (must be empty for this command)
 *
 * @see MAJOR_VERSION, MINOR_VERSION and PATCH_VERSION in Makefile.
 *
 * @return zero or positive integer if success, negative integer otherwise.
 *
 */
void handler_get_version(const buffer_t *data_buffer);
