#ifndef _GET_APP_NAME_H_
#define _GET_APP_NAME_H_

#include <stdint.h>

#include "../types.h"

/**
 * Function handling GET_APP_NAME command.
 *
 * @brief send APDU response with ASCII encoded name of the application
 * as in APPNAME variable of the Makefile.
 *
 * @param cdata buffer with APDU command data.
 * @param cdata_len length of APDU command data buffer.
 *
 * @return positive integer if success, -1 otherwise.
 *
 * @throw SW_APPNAME_TOO_LONG if len(APPNAME) > 64.
 *
 */
int get_app_name(uint8_t *cdata, uint8_t cdata_len);

#endif  // _GET_APP_NAME_H_
