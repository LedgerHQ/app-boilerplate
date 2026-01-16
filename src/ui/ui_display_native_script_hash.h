#pragma once

typedef enum {
    UI_SCRIPT_PUBKEY_PATH = 0,  // aka DEVICE_OWNED
    UI_SCRIPT_PUBKEY_HASH,      // aka THIRD_PARTY
    UI_SCRIPT_ALL,
    UI_SCRIPT_ANY,
    UI_SCRIPT_N_OF_K,
    UI_SCRIPT_INVALID_BEFORE,
    UI_SCRIPT_INVALID_HEREAFTER,
} ui_native_script_type;


#include "securityPolicy.h"
//TODO: add warning bits
/**
 * Display native script hash
 *
 * @param securityPolicy Security policy result
 * @param warnings Warning bits
 * @return 0 if success, negative integer otherwise
 */
int ui_display_native_script_hash(security_policy_t securityPolicy);