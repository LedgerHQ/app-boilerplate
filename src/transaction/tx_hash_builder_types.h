#pragma once

#include <stdint.h>

#include "cardano_constants.h"

typedef enum {
    CREDENTIAL_KEY_HASH = 0,
    CREDENTIAL_SCRIPT_HASH = 1,
} credential_type_t;

typedef struct {
    credential_type_t type;
    union {
        uint8_t keyHash[ADDRESS_KEY_HASH_LENGTH];
        uint8_t scriptHash[SCRIPT_HASH_LENGTH];
    };
} credential_t;
