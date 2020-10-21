#ifndef _DISPATCHER_H_
#define _DISPATCHER_H_

#include <stdint.h>

#include "types.h"

int dispatch(cmd_e ins, uint8_t p1, uint8_t p2, const buf_t *input);

#endif  // _DISPATCHER_H_
