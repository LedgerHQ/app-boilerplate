#pragma once

enum {
    STAGE_COMPLEX_SCRIPT_START = 0x01,
    STAGE_ADD_SIMPLE_SCRIPT = 0x02,
    STAGE_WHOLE_NATIVE_SCRIPT_FINISH = 0x03,
};

typedef enum {
    DISPLAY_NATIVE_SCRIPT_HASH_BECH32 = 1,
    DISPLAY_NATIVE_SCRIPT_HASH_POLICY_ID = 2,
} display_format;

typedef enum {
    UI_SCRIPT_INIT,
    UI_SCRIPT_CONTINUE,
    UI_SCRIPT_PUBKEY_PATH,  // aka DEVICE_OWNED
    UI_SCRIPT_PUBKEY_HASH,      // aka THIRD_PARTY
    UI_SCRIPT_ALL,
    UI_SCRIPT_ANY,
    UI_SCRIPT_N_OF_K,
    UI_SCRIPT_INVALID_BEFORE,
    UI_SCRIPT_INVALID_HEREAFTER,
    UI_SCRIPT_DISPLAY_BECH32,
    UI_SCRIPT_DISPLAY_POLICY_ID,
    UI_SCRIPT_FINISHED
} ui_native_script_type;

//TODO: comments
/**
 * Handler for INS_DERIVE_NATIVE_SCRIPT_HASH command. Send APDU response with ASCII
 * encoded name of the application.
 *
 * @see variable APPNAME in Makefile.
 *
 * @return zero or positive integer if success, negative integer otherwise.
 *
 */
int handler_derive_native_script_hash(buffer_t *cdata, uint8_t script_type);