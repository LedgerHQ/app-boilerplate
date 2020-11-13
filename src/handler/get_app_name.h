#pragma once

/**
 * Function handling GET_APP_NAME command.
 *
 * @brief send APDU response with ASCII encoded name of the application
 * as in APPNAME variable of the Makefile.
 *
 * @return zero or positive integer if success, negative number otherwise.
 *
 */
int handler_get_app_name(void);
