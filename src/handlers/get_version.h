#ifndef _GET_VERSION_H_
#define _GET_VERSION_H_

#include <stdint.h>

#include "../types.h"

/**
 * Function handling GET_VERSION command.
 *
 * @brief send APDU response with version of the application as MAJOR (1 byte),
 * MINOR (1 byte), PATCH (1 byte).
 *
 * @param p1 instruction P1 (1 byte).
 * @param p2 instruction P2 (1 byte).
 * @param input buffer buf_t with command data.
 *
 * @return positive integer if success, -1 otherwise.
 *
 */
int get_version(uint8_t p1, uint8_t p2, const buf_t *input);

#endif  // _GET_VERSION_H_
