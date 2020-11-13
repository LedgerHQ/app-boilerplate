#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <cmocka.h>

#include "common/format.h"

static void test_format(void **state) {
    (void) state;

    char temp[21] = {0};

    int64_t value = 0;
    assert_true(format_i64(temp, sizeof(temp), value));
    assert_string_equal(temp, "0");

    value = (int64_t) 9223372036854775807ull;  // MAX_INT64
    memset(temp, 0, sizeof(temp));
    assert_true(format_i64(temp, sizeof(temp), value));
    assert_string_equal(temp, "9223372036854775807");

    // buffer too small
    assert_false(format_i64(temp, sizeof(temp) - 5, value));

    value = (int64_t) -9223372036854775808ull;  // MIN_INT64
    memset(temp, 0, sizeof(temp));
    assert_true(format_i64(temp, sizeof(temp), value));
    assert_string_equal(temp, "-9223372036854775808");

    uint64_t amount = 100000000ull;  // satoshi
    memset(temp, 0, sizeof(temp));
    assert_true(format_fpu64(temp, sizeof(temp), amount, 8));
    assert_string_equal(temp, "1.00000000");  // BTC

    amount = 24964823ull;  // satoshi
    memset(temp, 0, sizeof(temp));
    assert_true(format_fpu64(temp, sizeof(temp), amount, 8));
    assert_string_equal(temp, "0.24964823");  // BTC

    amount = 100ull;  // satoshi
    memset(temp, 0, sizeof(temp));
    assert_true(format_fpu64(temp, sizeof(temp), amount, 8));
    assert_string_equal(temp, "0.00000100");  // BTC
    // buffer too small
    assert_false(format_fpu64(temp, sizeof(temp) - 16, amount, 8));

    char temp2[50] = {0};

    amount = 1000000000000000000ull;  // wei
    assert_true(format_fpu64(temp2, sizeof(temp2), amount, 18));
    assert_string_equal(temp2, "1.000000000000000000");  // ETH

    // buffer too small
    assert_false(format_fpu64(temp2, sizeof(temp2) - 20, amount, 18));
    // overflow
    assert_false(format_fpu64(temp2, sizeof(temp2), UINT64_MAX, 18));
}

int main() {
    const struct CMUnitTest tests[] = {cmocka_unit_test(test_format)};

    return cmocka_run_group_tests(tests, NULL, NULL);
}
