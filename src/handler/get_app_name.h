#pragma once

#include "os.h"
#include "buffer.h"

#define APPNAME_LEN (sizeof(APPNAME) - 1)
#define MAX_APP_NAME_LENGTH 64

//TODO: modify return in comment
/**
 * Handler for INS_GET_APP_NAME command. Send APDU response with ASCII
 * encoded name of the application.
 *
 * @param[in] data_buffer
 *   Buffer containing APDU data (must be empty for this command)
 *
 * @see variable APPNAME in Makefile.
 *
 * @return zero or positive integer if success, negative integer otherwise.
 *
 */
void handler_get_app_name(const buffer_t *data_buffer);
