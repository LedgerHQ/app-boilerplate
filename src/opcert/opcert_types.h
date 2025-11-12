#pragma once

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t

#include "addressUtils/bip44.h"

#define KES_PUBLIC_KEY_LENGTH 32

typedef struct {
    uint8_t *kesPublicKey;
    uint64_t kesPeriod;
    uint64_t issueCounter;
    bip44_path_t poolColdKeyPath;
} parsed_opcert_t;
