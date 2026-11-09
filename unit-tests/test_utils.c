/*
 * Unit tests for src/transaction/utils.c.
 *
 * Encoding check + memo formatting. is_printable_string() of the SDK is a
 * CMock mock: the test checks what the encoding check forwards to it.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity.h"

#include "Mockos_utils.h"

#include "transaction/utils.h"
#include "types.h"  // MAX_MEMO_LEN

static const char *g_printable_str;
static size_t g_printable_len;
static int g_printable_calls;
static bool g_printable_ret;

static bool printable_cb(const char *str, size_t len, int cmock_num_calls) {
    (void) cmock_num_calls;
    g_printable_str = str;
    g_printable_len = len;
    g_printable_calls++;
    return g_printable_ret;
}

void setUp(void) {
    Mockos_utils_Init();
    is_printable_string_Stub(printable_cb);
    g_printable_str = NULL;
    g_printable_len = 0;
    g_printable_calls = 0;
    g_printable_ret = true;
}

void tearDown(void) {
    Mockos_utils_Verify();
    Mockos_utils_Destroy();
}

// =========================================================================
// transaction_utils_check_encoding: forwards the memo to is_printable_string()
// =========================================================================

void test_check_encoding_forwards_memo(void) {
    const uint8_t memo[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x21};  // "Hello!"

    g_printable_ret = true;
    TEST_ASSERT_TRUE(transaction_utils_check_encoding(memo, sizeof(memo)));
    TEST_ASSERT_EQUAL(1, g_printable_calls);
    TEST_ASSERT_EQUAL_PTR(memo, g_printable_str);
    TEST_ASSERT_EQUAL(sizeof(memo), g_printable_len);

    g_printable_ret = false;
    TEST_ASSERT_FALSE(transaction_utils_check_encoding(memo, sizeof(memo)));
    TEST_ASSERT_EQUAL(2, g_printable_calls);
}

void test_check_encoding_empty_memo(void) {
    const uint8_t memo[] = {0x48};

    TEST_ASSERT_TRUE(transaction_utils_check_encoding(memo, 0));
    TEST_ASSERT_EQUAL(1, g_printable_calls);
    TEST_ASSERT_EQUAL(0, g_printable_len);
}

void test_check_encoding_max_memo_len(void) {
    static uint8_t memo[MAX_MEMO_LEN];

    TEST_ASSERT_TRUE(transaction_utils_check_encoding(memo, MAX_MEMO_LEN));
    TEST_ASSERT_EQUAL(MAX_MEMO_LEN, g_printable_len);
}

void test_check_encoding_too_long_memo(void) {
    static uint8_t memo[MAX_MEMO_LEN + 1];

    // Refused before the SDK call, including a length truncated by a 32-bit size_t.
    TEST_ASSERT_FALSE(transaction_utils_check_encoding(memo, MAX_MEMO_LEN + 1));
    TEST_ASSERT_FALSE(transaction_utils_check_encoding(memo, (uint64_t) 1 << 32));
    TEST_ASSERT_EQUAL(0, g_printable_calls);
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

    RUN_TEST(test_check_encoding_forwards_memo);
    RUN_TEST(test_check_encoding_empty_memo);
    RUN_TEST(test_check_encoding_max_memo_len);
    RUN_TEST(test_check_encoding_too_long_memo);
    RUN_TEST(test_format_memo);

    return UNITY_END();
}
