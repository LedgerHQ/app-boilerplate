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
 * @param cdata buffer with APDU command data.
 * @param cdata_len length of APDU command data buffer.
 *
 * @return positive integer if success, -1 otherwise.
 *
 * @throw SW_APPVERSION_WRONG_LENGTH if len(MAJOR || MINOR || PATCH) != 3.
 *
 */
int get_version(uint8_t *cdata, uint8_t cdata_len);

#endif  // _GET_VERSION_H_
