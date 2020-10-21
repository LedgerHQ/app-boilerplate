#ifndef _TYPES_H_
#define _TYPES_H_

#include <stddef.h>
#include <stdint.h>

typedef enum { READY, RECEIVED, WAITING } io_state_e;

typedef struct {
    uint8_t *bytes;
    size_t size;
} buf_t;

typedef enum {
    GET_VERSION = 0x03,
    GET_APP_NAME = 0x04,
} cmd_e;

#endif  // _TYPES_H_
