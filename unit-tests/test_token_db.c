/*
 * Unit tests for src/token/token_db.c.
 *
 * get_hardcoded_token_info() scans the static TOKENS[] table and fills the
 * ticker/decimals on a 32-byte address match. Pure logic (memcmp over a static
 * table), so no mocks. The reference addresses below mirror the entries in
 * token_db.c (the table itself is file-private); we cover the first and last
 * entries (so the whole scan is exercised) plus a miss.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity.h"

#include "token_db.h"
#include "types.h"  // token_info_t, TOKEN_ADDRESS_LEN

// First TOKENS[] entry: "USDC", 12 decimals.
static const uint8_t USDC_ADDRESS[TOKEN_ADDRESS_LEN] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};

// Last TOKENS[] entry: "LINK", 14 decimals.
static const uint8_t LINK_ADDRESS[TOKEN_ADDRESS_LEN] = {
    0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00,
    0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00};

void setUp(void) {
}

void tearDown(void) {
}

void test_get_hardcoded_token_info_first_entry(void) {
    token_info_t info = {0};

    TEST_ASSERT_TRUE(get_hardcoded_token_info(USDC_ADDRESS, &info));
    TEST_ASSERT_EQUAL_STRING("USDC", info.ticker);
    TEST_ASSERT_EQUAL(12, info.decimals);
}

void test_get_hardcoded_token_info_last_entry(void) {
    token_info_t info = {0};

    TEST_ASSERT_TRUE(get_hardcoded_token_info(LINK_ADDRESS, &info));
    TEST_ASSERT_EQUAL_STRING("LINK", info.ticker);
    TEST_ASSERT_EQUAL(14, info.decimals);
}

void test_get_hardcoded_token_info_not_found(void) {
    // Address absent from the table.
    uint8_t unknown[TOKEN_ADDRESS_LEN];
    memset(unknown, 0x00, sizeof(unknown));

    token_info_t info = {0};
    TEST_ASSERT_FALSE(get_hardcoded_token_info(unknown, &info));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_get_hardcoded_token_info_first_entry);
    RUN_TEST(test_get_hardcoded_token_info_last_entry);
    RUN_TEST(test_get_hardcoded_token_info_not_found);

    return UNITY_END();
}
