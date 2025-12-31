#pragma once

#include <stdint.h>

typedef enum {
    WARNING_BIT_UNUSUAL_KEY_DERIVATION_PATH = 0,
    WARNING_BIT_NETWORK_UNUSUAL,
    WARNING_BIT_NETWORK_NOT_VERIFIABLE,
    WARNING_BIT_PLUTUS_PRESENT,
    WARNING_BIT_PLUTUS_MISSING_COLLATERAL,
    WARNING_BIT_PLUTUS_UNKNOWN_COLLATERAL,
    WARNING_BIT_PLUTUS_MISSING_SCRIPT_DATA_HASH,
    WARNING_BIT_OUTPUT_MISSING_DATUM,
    WARNING_BIT_CVOTE_PAYMENT_THIRD_PARTY,
    WARNING_BIT_CVOTE_PAYMENT_NONSTANDARD_OWNED,
    WARNING_BIT_POOL_REGISTRATION_NO_OWNERS,
    WARNING_BIT_POOL_REGISTRATION_NO_RELAYS,
    WARNING_BIT_HIGH_FEE,
    WARNING_BIT_COUNT,
} warning_bit_e;

typedef uint64_t warning_bits_t;

static inline void warning_bits_init(warning_bits_t* warnings) {
    *warnings = 0;
}

static inline void warning_bits_set(warning_bits_t* warnings, warning_bit_e bit) {
    *warnings |= (warning_bits_t)1 << bit;
}

static inline bool warning_bits_has(warning_bits_t warnings, warning_bit_e bit) {
    return ((warnings >> bit) & 1) != 0;
}
