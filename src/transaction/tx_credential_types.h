#pragma once

#include "cardano_constants.h"
#include "addressUtils/bip44.h"

typedef enum {
    EXT_CREDENTIAL_KEY_HASH = 0,      // Wire: 0x02, CBOR: 0
    EXT_CREDENTIAL_SCRIPT_HASH = 1,   // Wire: 0x01, CBOR: 1
    EXT_CREDENTIAL_KEY_PATH = 2,      // Wire: 0x00, not in CBOR (converted to KEY_HASH via bip44_pathToKeyHash)
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
    EXT_DREP_SCRIPT_HASH = 1,
    EXT_DREP_ABSTAIN = 2,
    EXT_DREP_NO_CONFIDENCE = 3,
    EXT_DREP_KEY_PATH = 4,  // internal-only marker for host-supplied paths
} ext_drep_type_t;

// Extended DREP structure
typedef struct {
    ext_drep_type_t type;
    union {
        bip44_path_t keyPath;
        uint8_t keyHash[ADDRESS_KEY_HASH_LENGTH];
        uint8_t scriptHash[SCRIPT_HASH_LENGTH];
    };
} ext_drep_t;
