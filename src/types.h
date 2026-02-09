#pragma once

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t

#include "bip32.h"

#include "constants.h"

/**
 * Enumeration with expected INS of APDU commands.
 */
typedef enum {
    GET_APP_NAME = 0x04,       /// name of the application
} command_e;

/**
 * Enumeration with parsing state.
 */
typedef enum {
    STATE_NONE,     /// No state
} state_e;

/**
 * Enumeration with user request type.
 */
typedef enum {
    REQUEST_NONE, // No request
} request_type_e;

/**
 * Structure for global context.
 */
typedef struct {
    state_e state;  /// state of the context
    request_type_e req_type;              /// user request
    uint32_t bip32_path[MAX_BIP32_PATH];  /// BIP32 path
    uint8_t bip32_path_len;               /// length of BIP32 path
} global_ctx_t;
