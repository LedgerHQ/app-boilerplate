#pragma once

#include "globals.h"
#include "utils/assert.h"

enum {
    STORAGE_INITIALIZED = 0x01
};

enum {
    SETTINGS_NO = 0,
    SETTINGS_YES = 1
};

static inline uint8_t flip_bool_setting(uint8_t value)
{
    switch (value) {
    case SETTINGS_NO:
        return SETTINGS_YES;
    case SETTINGS_YES:
        return SETTINGS_NO;
    default:
        ASSERT(false);
        return 0;  // Unreachable, but satisfies compiler
    }
}

static inline bool is_expert_mode()
{
    return N_storage.expert_mode_enabled;
}

static inline bool is_silent_pubkey_export_allowed()
{
    return N_storage.silent_pubkey_export_enabled;
}
