#pragma once

/** Instruction class byte for the Cardano app. */
#define CLA 0xD7

/**
 * Expected INS values for APDU commands.
 */
typedef enum {
    INS_GET_SERIAL = 0x01,
    INS_GET_VERSION = 0x03,
    INS_GET_APP_NAME = 0x04,
    INS_GET_PUBLIC_KEY = 0x10,
    INS_DERIVE_ADDRESS = 0x11,
    INS_DERIVE_NATIVE_SCRIPT_HASH = 0x12,
    INS_SIGN_TX = 0x21,
    INS_SIGN_OPCERT = 0x22,
#ifdef DEBUG
    INS_DEBUG_SET_SETTINGS = 0xF0,  // Debug-only command for testing
#endif
} command_e;

/**
 * Request types handled by the dispatcher.
 */
typedef enum {
    REQUEST_NONE = 0,
    REQUEST_EXPORT_PUBKEY,
    REQUEST_SIGN_TRANSACTION,
    REQUEST_SIGN_OPCERT,
} request_type_e;
