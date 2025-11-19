#pragma once

#include <stdint.h>   // uint*_t
#include <stdbool.h>  // bool
#include "types.h"    // bool

/* Check if the Tx to sign have the same parameters as the ones previously validated */
bool swap_check_validity(uint64_t amount,
                         uint64_t fee,
                         const uint8_t* destination,
                         const token_info_t* token_info);
