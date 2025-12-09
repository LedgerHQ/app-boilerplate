#pragma once

#include "io.h"
#include "globals.h"

/**
 * Helper function to safely reset context and return an error status.
 *
 * This should be called when an error occurs during instruction processing to ensure
 * the application returns to a clean idle state. It:
 * - Resets req_type to REQUEST_NONE (no operation in progress)
 * - Securely clears the state union (prevents information leaks)
 * - Sends the given status word to the host
 *
 * @param sw Status word to return to the host
 * @return Result of io_send_sw()
 */
static inline int send_error_and_reset(uint16_t sw) {
    G_context.req_type = REQUEST_NONE;
    explicit_bzero(&G_context.state, sizeof(G_context.state));
    return io_send_sw(sw);
}
