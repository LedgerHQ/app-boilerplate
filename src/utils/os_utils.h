#pragma once

#include <stdint.h>
#include <string.h>
#include "io.h"
#include "os.h"
#include "globals.h"
#include "assert.h"
#include "utils.h"

// Macro for accessing PIC (Program counter) relative data
// Uses os.h which is only needed here
#define PTR_PIC(ptr) ((__typeof__(ptr)) PIC(ptr))

/**
 * Helper: Reset context to idle and return error
 * Call this on any error during instruction processing to ensure clean state
 * Sets req_type = REQUEST_NONE and state = *_STATE_NONE (union member)
 * Works for all instructions (transaction, opcert, pubkey, etc.)
 */
static inline int send_error_and_reset(uint16_t sw) {
    G_context.req_type = REQUEST_NONE;
    explicit_bzero(&G_context.state, SIZEOF(G_context.state));
    return io_send_sw(sw);
}
