#ifndef _DISPATCHER_H_
#define _DISPATCHER_H_

#include "../types.h"

/**
 * Function to dispatch all commands.
 *
 * @brief redirect APDU command received to the right handler.
 *
 * @param cmd APDU command (CLA, INS, P1, P2, Lc, Command data).
 *
 * @return positive integer if success, -1 otherwise.
 *
 */
int dispatch_command(const command_t *cmd);

#endif  // _DISPATCHER_H_
