#pragma once

/**
 * Function handling GET_VERSION command.
 *
 * @brief send APDU response with version of the application as: MAJOR (1 byte) ||
 * MINOR (1 byte) || PATCH (1 byte).
 *
 * @return zero or positive integer if success, negative number otherwise.
 *
 */
int handler_get_version(void);
