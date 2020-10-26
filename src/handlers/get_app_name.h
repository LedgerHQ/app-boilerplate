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
 * @param p1 instruction P1 (1 byte).
 * @param p2 instruction P2 (1 byte).
 * @param input buffer buf_t with command data.
 *
 * @return positive integer if success, -1 otherwise.
 *
 * @throw SW_APPNAME_TOO_LONG if len(APPNAME) > 64.
 *
 */
int get_app_name(uint8_t p1, uint8_t p2, const buf_t *input);

#endif  // _GET_APP_NAME_H_
