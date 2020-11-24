#pragma once

#include "../types.h"

#define P2_LAST  0x00
#define P2_MORE  0x80
#define P1_START 0x00
#define P1_MAX   0x03

/**
 * Function to dispatch APDU command.
 *
 * @brief redirect APDU command received to the right handler.
 *
 * @param[in] cmd APDU command (CLA, INS, P1, P2, Lc, Command data).
 *
 * @return zero or positive integer if success, negative otherwise.
 *
 */
int apdu_dispatcher(const command_t *cmd);
