#pragma once

#include <stdint.h>  // uint*_t
#include "hash.h"
#include "addressUtils/bip44.h"

#define MAX_SCRIPT_DEPTH 11

typedef enum {
    NATIVE_SCRIPT_PUBKEY = 0,
    NATIVE_SCRIPT_ALL = 1,
    NATIVE_SCRIPT_ANY = 2,
    NATIVE_SCRIPT_N_OF_K = 3,
    NATIVE_SCRIPT_INVALID_BEFORE = 4,
    NATIVE_SCRIPT_INVALID_HEREAFTER = 5,
} native_script_type;

typedef enum {
    NATIVE_SCRIPT_HASH_BUILDER_SCRIPT = 100,
    NATIVE_SCRIPT_HASH_BUILDER_FINISHED = 200,
} native_script_hash_builder_state_t;

typedef struct {
    uint8_t level;
    uint32_t remainingScripts[MAX_SCRIPT_DEPTH];
    native_script_hash_builder_state_t state;
    blake2b_224_context_t nativeScriptHash;
} native_script_hash_builder_t;

typedef struct {
    uint32_t totalScripts;
    uint32_t remainingScripts;
} complex_native_script_t;

typedef union {
    uint32_t requiredScripts;
    bip44_path_t pubkeyPath;
    uint8_t pubkeyHash[ADDRESS_KEY_HASH_LENGTH];
    uint64_t timelock;
} native_script_content_t;
