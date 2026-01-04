#pragma once

#include <stdint.h>
#include "utils/utils.h"

typedef enum {
    WARNING_BIT_UNUSUAL_KEY_DERIVATION_PATH = 0,
    WARNING_BIT_NETWORK_UNUSUAL,
    WARNING_BIT_NETWORK_NOT_VERIFIABLE,
    WARNING_BIT_PLUTUS_MISSING_COLLATERAL,
    WARNING_BIT_PLUTUS_UNKNOWN_COLLATERAL,
    WARNING_BIT_COLLATERAL_OUTPUT_WARNING,
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

static inline const char* warning_bit_name(warning_bit_e bit) {
    static const char* const names[WARNING_BIT_COUNT] = {
        [WARNING_BIT_UNUSUAL_KEY_DERIVATION_PATH] = "WARNING_BIT_UNUSUAL_KEY_DERIVATION_PATH",
        [WARNING_BIT_NETWORK_UNUSUAL] = "WARNING_BIT_NETWORK_UNUSUAL",
        [WARNING_BIT_NETWORK_NOT_VERIFIABLE] = "WARNING_BIT_NETWORK_NOT_VERIFIABLE",
        [WARNING_BIT_PLUTUS_MISSING_COLLATERAL] = "WARNING_BIT_PLUTUS_MISSING_COLLATERAL",
        [WARNING_BIT_PLUTUS_UNKNOWN_COLLATERAL] = "WARNING_BIT_PLUTUS_UNKNOWN_COLLATERAL",
        [WARNING_BIT_COLLATERAL_OUTPUT_WARNING] = "WARNING_BIT_COLLATERAL_OUTPUT_WARNING",
        [WARNING_BIT_PLUTUS_MISSING_SCRIPT_DATA_HASH] = "WARNING_BIT_PLUTUS_MISSING_SCRIPT_DATA_HASH",
        [WARNING_BIT_OUTPUT_MISSING_DATUM] = "WARNING_BIT_OUTPUT_MISSING_DATUM",
        [WARNING_BIT_CVOTE_PAYMENT_THIRD_PARTY] = "WARNING_BIT_CVOTE_PAYMENT_THIRD_PARTY",
        [WARNING_BIT_CVOTE_PAYMENT_NONSTANDARD_OWNED] = "WARNING_BIT_CVOTE_PAYMENT_NONSTANDARD_OWNED",
        [WARNING_BIT_POOL_REGISTRATION_NO_OWNERS] = "WARNING_BIT_POOL_REGISTRATION_NO_OWNERS",
        [WARNING_BIT_POOL_REGISTRATION_NO_RELAYS] = "WARNING_BIT_POOL_REGISTRATION_NO_RELAYS",
        [WARNING_BIT_HIGH_FEE] = "WARNING_BIT_HIGH_FEE",
    };

    if ((unsigned)bit >= WARNING_BIT_COUNT) {
        return "WARNING_BIT_UNKNOWN";
    }

    const char* name = names[bit];
    if (name == NULL) {
        return "WARNING_BIT_UNKNOWN";
    }

    return name;
}

static inline void warning_bits_init(warning_bits_t* warnings) {
    *warnings = 0;
}

#ifdef DEBUG
static inline void _trace_warning_bit(warning_bit_e bit) {
    switch (bit) {
        case WARNING_BIT_UNUSUAL_KEY_DERIVATION_PATH:
            TRACE("Warning: UNUSUAL_KEY_DERIVATION_PATH"); break;
        case WARNING_BIT_NETWORK_UNUSUAL:
            TRACE("Warning: NETWORK_UNUSUAL"); break;
        case WARNING_BIT_NETWORK_NOT_VERIFIABLE:
            TRACE("Warning: NETWORK_NOT_VERIFIABLE"); break;
        case WARNING_BIT_PLUTUS_MISSING_COLLATERAL:
            TRACE("Warning: PLUTUS_MISSING_COLLATERAL"); break;
        case WARNING_BIT_PLUTUS_UNKNOWN_COLLATERAL:
            TRACE("Warning: PLUTUS_UNKNOWN_COLLATERAL"); break;
        case WARNING_BIT_COLLATERAL_OUTPUT_WARNING:
            TRACE("Warning: COLLATERAL_OUTPUT_WARNING"); break;
        case WARNING_BIT_PLUTUS_MISSING_SCRIPT_DATA_HASH:
            TRACE("Warning: PLUTUS_MISSING_SCRIPT_DATA_HASH"); break;
        case WARNING_BIT_OUTPUT_MISSING_DATUM:
            TRACE("Warning: OUTPUT_MISSING_DATUM"); break;
        case WARNING_BIT_CVOTE_PAYMENT_THIRD_PARTY:
            TRACE("Warning: CVOTE_PAYMENT_THIRD_PARTY"); break;
        case WARNING_BIT_CVOTE_PAYMENT_NONSTANDARD_OWNED:
            TRACE("Warning: CVOTE_PAYMENT_NONSTANDARD_OWNED"); break;
        case WARNING_BIT_POOL_REGISTRATION_NO_OWNERS:
            TRACE("Warning: POOL_REGISTRATION_NO_OWNERS"); break;
        case WARNING_BIT_POOL_REGISTRATION_NO_RELAYS:
            TRACE("Warning: POOL_REGISTRATION_NO_RELAYS"); break;
        case WARNING_BIT_HIGH_FEE:
            TRACE("Warning: HIGH_FEE"); break;
        default:
            TRACE("Warning: UNKNOWN bit=%u", (unsigned int)bit); break;
    }
}
#else
#define _trace_warning_bit(bit) do { (void)(bit); } while(0)
#endif

static inline void warning_bits_set(warning_bits_t* warnings, warning_bit_e bit) {
    *warnings |= (warning_bits_t)1 << bit;
    _trace_warning_bit(bit);
}

static inline bool warning_bits_has(warning_bits_t warnings, warning_bit_e bit) {
    return ((warnings >> bit) & 1) != 0;
}
