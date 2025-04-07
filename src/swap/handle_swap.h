#pragma once

#include "tx_types.h"

typedef struct swap_validated_s {
    bool initialized;
    uint64_t amount;
    uint64_t fee;
    char recipient[ADDRESS_LEN * 2 + 1];
} swap_validated_t;

extern swap_validated_t G_swap_validated;

bool swap_check_validity(bool initialized,
                         uint64_t amount,
                         uint64_t fee,
                         const char *recipient);