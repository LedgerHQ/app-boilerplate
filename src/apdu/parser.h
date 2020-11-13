#pragma once

#include <stddef.h>   // size_t
#include <stdint.h>   // uint*_t
#include <stdbool.h>  // bool

#include "../types.h"

/**
 * Function to parse APDU command.
 *
 * @brief parse input buffer into command_t struct.
 *
 * @param[out] cmd command_t struct with APDU command fields.
 * @param[in]  buf bytes buffer within APDU command.
 * @param[in]  buf_len length of bytes buffer.
 *
 * @return true if success, false otherwise.
 *
 */
bool apdu_parser(command_t *cmd, uint8_t *buf, size_t buf_len);
