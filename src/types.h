#pragma once

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t

#include "bip32.h"

#include "constants.h"

/**
 * Enumeration with expected INS of APDU commands.
 */
typedef enum {
    BENCH_PRIME = 0x01,
    BENCH_FIBO = 0x02,
    BENCH_BW = 0x03,
} command_e;

typedef enum {
    BW_TYPE_IN = 0x00,
    BW_TYPE_OUT = 0x01,
    BW_TYPE_BIDIR = 0x02,
} bw_type_e;
