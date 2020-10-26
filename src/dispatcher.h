#ifndef _DISPATCHER_H_
#define _DISPATCHER_H_

#include <stdint.h>

#include "types.h"

/**
 * Function dispatching all commands.
 *
 * @brief redirect APDU command received using INS to the right handler.
 *
 * @param ins instruction code INS (1 byte).
 * @param p1 instruction P1 (1 byte).
 * @param p2 instruction P2 (1 byte).
 * @param input buffer buf_t with command data.
 *
 * @return positive integer if success, -1 otherwise.
 *
 */
int dispatch(cmd_e ins, uint8_t p1, uint8_t p2, const buf_t *input);

#endif  // _DISPATCHER_H_
