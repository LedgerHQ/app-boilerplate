#pragma once

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t

#include "addressUtils/bip44.h"

#define KES_PUBLIC_KEY_LENGTH 32
#define OPCERT_KES_PERIOD_SIZE 8   // uint64_t
#define OPCERT_ISSUE_COUNTER_SIZE 8  // uint64_t

typedef struct {
    uint8_t *kesPublicKey;
    uint64_t kesPeriod;
    uint64_t issueCounter;
    bip44_path_t poolColdKeyPath;
} parsed_opcert_t;
