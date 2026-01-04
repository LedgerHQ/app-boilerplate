#pragma once

#ifdef DEBUG

#include <stddef.h>  // size_t
#include "buffer.h"

/**
 * Handler for DEBUG-only APDU to set app settings for testing.
 *
 * This command is only available in DEBUG builds and allows tests to
 * programmatically set app settings without UI navigation.
 *
 * @param[in] buf Buffer containing settings data:
 *                - byte 0: expert_mode_enabled (0x00 = off, 0x01 = on)
 *                - byte 1: silent_pubkey_export_enabled (0x00 = off, 0x01 = on)
 *
 * @return zero or positive integer if success, negative integer otherwise.
 */
int handler_debug_set_settings(const buffer_t *buf);

#endif  // DEBUG
