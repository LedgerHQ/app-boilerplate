#pragma once

#define STATIC_ASSERT _Static_assert

#define ASSERT_TYPE(expr, expected_type) \
    STATIC_ASSERT(__builtin_types_compatible_p(__typeof__((expr)), expected_type), "Wrong type")

// TODO adding individual messages to all places where we use ASSERT might be overkill
#define ASSERT(x) LEDGER_ASSERT((x), "probably a bug")

#if defined(TEST) || defined(FUZZ)
#include "assert.h"
#define LEDGER_ASSERT(x, y) assert(x)
#else
#include "ledger_assert.h"
#endif
