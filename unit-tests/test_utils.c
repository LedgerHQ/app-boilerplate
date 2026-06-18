/*
 * Unit tests for src/transaction/utils.c.
 *
 * Pure helpers (ASCII encoding check + memo formatting); no external
 * dependency, so no mocks. Functions come from the `app` library.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity.h"

#include "transaction/utils.h"
#include "types.h"  // MAX_MEMO_LEN

void setUp(void) {
}

void tearDown(void) {
}

// =========================================================================
// transaction_utils_check_encoding: true iff every byte is 7-bit ASCII
// =========================================================================

void test_check_encoding(void) {
    const uint8_t good_ascii[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x21};  // "Hello!"
    const uint8_t bad_ascii[] = {0x32, 0xc3, 0x97, 0x32, 0x3d, 0x34};   // "2×2=4"

    TEST_ASSERT_TRUE(transaction_utils_check_encoding(good_ascii, sizeof(good_ascii)));
    TEST_ASSERT_FALSE(transaction_utils_check_encoding(bad_ascii, sizeof(bad_ascii)));

    // Empty input: the loop never runs -> valid.
    TEST_ASSERT_TRUE(transaction_utils_check_encoding(good_ascii, 0));

    // Boundary: 0x7F is the last valid ASCII byte, 0x80 the first invalid one.
    const uint8_t edge_ok[] = {0x7F};
    const uint8_t edge_ko[] = {0x80};
    TEST_ASSERT_TRUE(transaction_utils_check_encoding(edge_ok, sizeof(edge_ok)));
    TEST_ASSERT_FALSE(transaction_utils_check_encoding(edge_ko, sizeof(edge_ko)));
}

// =========================================================================
// transaction_utils_format_memo: copy memo into dst as a NUL-terminated string
// =========================================================================

void test_format_memo(void) {
    const uint8_t memo[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x21};  // "Hello!"
    char dst[MAX_MEMO_LEN] = {0};

    // Nominal case.
    TEST_ASSERT_TRUE(transaction_utils_format_memo(memo, sizeof(memo), dst, sizeof(dst)));
    TEST_ASSERT_EQUAL_STRING("Hello!", dst);

    // Exact fit: dst_len == memo_len + 1 (room for the trailing NUL).
    char tight[sizeof(memo) + 1] = {0};
    TEST_ASSERT_TRUE(transaction_utils_format_memo(memo, sizeof(memo), tight, sizeof(tight)));
    TEST_ASSERT_EQUAL_STRING("Hello!", tight);

    // dst too small: dst_len == memo_len leaves no room for the NUL -> false.
    TEST_ASSERT_FALSE(transaction_utils_format_memo(memo, sizeof(memo), dst, sizeof(memo)));

    // memo longer than MAX_MEMO_LEN -> false (checked before any copy).
    TEST_ASSERT_FALSE(transaction_utils_format_memo(memo, MAX_MEMO_LEN + 1, dst, sizeof(dst)));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_check_encoding);
    RUN_TEST(test_format_memo);

    return UNITY_END();
}
