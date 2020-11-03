#ifndef _GET_VERSION_H_
#define _GET_VERSION_H_

#include <stdint.h>

#include "../types.h"

/**
 * Function handling GET_VERSION command.
 *
 * @brief send APDU response with version of the application as: MAJOR (1 byte) ||
 * MINOR (1 byte) || PATCH (1 byte).
 *
 * @return positive integer if success, -1 otherwise.
 *
 * @throw SW_APPVERSION_WRONG_LENGTH if len(MAJOR || MINOR || PATCH) != 3.
 *
 */
int get_version(void);

#endif  // _GET_VERSION_H_
