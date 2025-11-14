#pragma once

#include "constants.h"
#include "addressUtils/bip44.h"

// Extended credential type (allows key path, key hash, or script hash)
typedef enum {
    // enum values are affected by backwards-compatibility
    EXT_CREDENTIAL_KEY_PATH = 0,
    EXT_CREDENTIAL_KEY_HASH = 2,
    EXT_CREDENTIAL_SCRIPT_HASH = 1,
} ext_credential_type_t;

// Extended credential structure
typedef struct {
    ext_credential_type_t type;
    union {
        bip44_path_t keyPath;
        uint8_t keyHash[ADDRESS_KEY_HASH_LENGTH];
        uint8_t scriptHash[SCRIPT_HASH_LENGTH];
    };
} ext_credential_t;

// DReps are extended to allow key derivation paths
typedef enum {
    EXT_DREP_KEY_HASH = 0,
    EXT_DREP_KEY_PATH = 0 + 100,
    EXT_DREP_SCRIPT_HASH = 1,
    EXT_DREP_ABSTAIN = 2,
    EXT_DREP_NO_CONFIDENCE = 3,
} ext_drep_type_t;

// Extended DREP structure
typedef struct {
    ext_drep_type_t type;
    union {
        uint8_t keyHash[ADDRESS_KEY_HASH_LENGTH];
        bip44_path_t keyPath;
        uint8_t scriptHash[SCRIPT_HASH_LENGTH];
    };
} ext_drep_t;
