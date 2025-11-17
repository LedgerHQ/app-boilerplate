#pragma once

#define STATIC_ASSERT _Static_assert

#define ASSERT_TYPE(expr, expected_type) \
    STATIC_ASSERT(__builtin_types_compatible_p(__typeof__((expr)), expected_type), "Wrong type")

#include "ledger_assert.h"

// TODO adding individual messages to all places where we use ASSERT might be overkill
#define ASSERT(x) LEDGER_ASSERT((x), "probably a bug")
