/*
 * Unit tests for src/token/dynamic_token_info.c.
 *
 * get_token_info() routes a lookup: first the in-RAM dynamic token (set via
 * set_token_info), then the hardcoded database. get_hardcoded_token_info()
 * lives in token_db.c and is mocked, so we can assert the routing/priority
 * without that module. The dynamic-token logic itself is exercised through the
 * public set_token_info() / init_dynamic_token_storage() entry points (the
 * storage struct is file-private).
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity.h"

#include "Mocktoken_db.h"

#include "dynamic_token_info.h"
#include "types.h"  // token_info_t, TOKEN_ADDRESS_LEN

void setUp(void) {
    Mocktoken_db_Init();
    init_dynamic_token_storage();  // start each test with no dynamic token
}

void tearDown(void) {
    Mocktoken_db_Verify();
    Mocktoken_db_Destroy();
}

// Store a dynamic token (ticker + decimals) for the given 32-byte address.
static void provide_dynamic(const uint8_t *address, const char *tk, uint8_t decimals) {
    char ticker[MAX_TICKER_SIZE + 1] = {0};
    strncpy(ticker, tk, MAX_TICKER_SIZE);
    buffer_t buf = {.ptr = address, .size = TOKEN_ADDRESS_LEN, .offset = 0};
    set_token_info(decimals, &ticker, &buf);
}

// =========================================================================
// Guard clauses
// =========================================================================

void test_get_token_info_invalid_args(void) {
    uint8_t address[TOKEN_ADDRESS_LEN] = {0};
    token_info_t info = {0};

    // NULL args are rejected before any lookup (no mock expected).
    TEST_ASSERT_FALSE(get_token_info(NULL, &info));
    TEST_ASSERT_FALSE(get_token_info(address, NULL));
}

// =========================================================================
// Routing: dynamic first, hardcoded fallback
// =========================================================================

void test_get_token_info_routing(void) {
    uint8_t address[TOKEN_ADDRESS_LEN];
    memset(address, 0xAB, sizeof(address));
    uint8_t other_address[TOKEN_ADDRESS_LEN];
    memset(other_address, 0xCD, sizeof(other_address));
    token_info_t info;

    // No dynamic token yet -> falls back to the hardcoded DB (found).
    get_hardcoded_token_info_ExpectAnyArgsAndReturn(true);
    TEST_ASSERT_TRUE(get_token_info(address, &info));

    // No dynamic token, hardcoded misses too -> not found.
    get_hardcoded_token_info_ExpectAnyArgsAndReturn(false);
    TEST_ASSERT_FALSE(get_token_info(address, &info));

    // Provide a dynamic token for `address`; a matching lookup returns the
    // dynamic info and must NOT consult the hardcoded DB (no expectation set,
    // so CMock fails if get_hardcoded_token_info is called).
    provide_dynamic(address, "USDC", 6);
    memset(&info, 0, sizeof(info));
    TEST_ASSERT_TRUE(get_token_info(address, &info));
    TEST_ASSERT_EQUAL_STRING("USDC", info.ticker);
    TEST_ASSERT_EQUAL(6, info.decimals);

    // A different address does not match the dynamic token -> hardcoded fallback.
    get_hardcoded_token_info_ExpectAnyArgsAndReturn(true);
    TEST_ASSERT_TRUE(get_token_info(other_address, &info));
}

// =========================================================================
// init_dynamic_token_storage clears a previously received token
// =========================================================================

void test_init_clears_dynamic_token(void) {
    uint8_t address[TOKEN_ADDRESS_LEN];
    memset(address, 0xAB, sizeof(address));
    token_info_t info;

    provide_dynamic(address, "USDC", 6);
    init_dynamic_token_storage();  // received flag back to false

    // The dynamic token is gone -> the matching address now hits the fallback.
    get_hardcoded_token_info_ExpectAnyArgsAndReturn(false);
    TEST_ASSERT_FALSE(get_token_info(address, &info));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_get_token_info_invalid_args);
    RUN_TEST(test_get_token_info_routing);
    RUN_TEST(test_init_clears_dynamic_token);

    return UNITY_END();
}
