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
 * @return positive integer if success, -1 otherwise.
 *
 * @throw SW_APPNAME_TOO_LONG if len(APPNAME) > 64.
 *
 */
int get_app_name(void);

#endif  // _GET_APP_NAME_H_
