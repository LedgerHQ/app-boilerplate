#ifndef _APDU_PARSER_H_
#define _APDU_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include "../types.h"

/**
 * Function to parse APDU command.
 *
 * @brief parse buf and fill cmd struct with APDU command.
 *
 * @param cmd struct to be filled with APDU command in buf.
 * @param buf buffer of bytes within APDU command.
 * @param buf_len length of the buffer.
 *
 * @return positive integer if success, -1 otherwise.
 *
 */
int parse_apdu(command_t *cmd, uint8_t *buf, size_t buf_len);

#endif  // _APDU_PARSER_H_
