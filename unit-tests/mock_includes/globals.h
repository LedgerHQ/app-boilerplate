#pragma once

// Mock globals.h for unit tests

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// Mock global context (minimal version for testing)
typedef struct {
    uint8_t req_type;
    union {
        uint8_t dummy[256];
    } state;
} global_ctx_t;

extern global_ctx_t G_context;

#define REQUEST_NONE 0

// Mock explicit_bzero
#ifndef explicit_bzero
#define explicit_bzero(addr, size) memset((addr), 0, (size))
#endif
