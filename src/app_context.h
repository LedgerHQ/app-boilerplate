#pragma once

#include <stdint.h>

/**
 * Send APDU status word and reset the application context.
 *
 * @param swo Status word to return.
 * @return Result of io_send_sw().
 */
void send_swo_and_reset(uint16_t swo);

/**
 * Centrally reset application state.
 *
 * This function ensures that all resources are released and the application
 * returns to a clean idle state. It:
 * - Cleans up UI allocations and review state
 * - Resets the transient allocator
 * - Securely zeroes out the global context
 * - Resets request type to idle
 */
void reset_app_context(void);
