#pragma once

#include "os.h"

#define APPNAME_LEN (sizeof(APPNAME) - 1)
#define MAX_APP_NAME_LENGTH 64

/**
 * Handler for INS_GET_APP_NAME command. Send APDU response with ASCII
 * encoded name of the application.
 *
 * @see variable APPNAME in Makefile.
 *
 * @return zero or positive integer if success, negative integer otherwise.
 *
 */
void handler_get_app_name(void);
