/*
 * Unit tests for src/swap/handle_get_printable_amount.c.
 *
 * swap_handle_get_printable_amount() turns a raw amount + (optional) coin
 * configuration into "<formatted> <ticker>" in params->printable_amount, or
 * leaves it empty on error. All helpers are mocked: swap_str_to_u64 /
 * swap_parse_config (swap_utils.h) and format_fpu64 (format.h), driven by
 * callbacks so the formatted output and ticker are fully controlled.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity.h"

#include "Mockswap_utils.h"
#include "Mockformat.h"

#include "swap.h"  // get_printable_amount_parameters_t, swap_handle_get_printable_amount

#define FORMATTED "12.34"

static uint8_t g_amount[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xe8};
static uint8_t g_config[4] = {0xaa, 0xbb, 0xcc, 0xdd};

// swap_parse_config mock: yield ticker "USDC" with 6 decimals.
static bool parse_config_cb(const uint8_t *config,
                            uint8_t config_len,
                            char *ticker,
                            uint8_t ticker_buf_len,
                            uint8_t *decimals,
                            int cmock_num_calls) {
    (void) config;
    (void) config_len;
    (void) cmock_num_calls;
    strncpy(ticker, "USDC", ticker_buf_len - 1);
    ticker[ticker_buf_len - 1] = '\0';
    *decimals = 6;
    return true;
}

// format_fpu64 mock: always produce FORMATTED.
static bool fpu64_cb(char *dst, size_t dst_len, uint64_t value, uint8_t decimals, int cmock_num_calls) {
    (void) value;
    (void) decimals;
    (void) cmock_num_calls;
    strncpy(dst, FORMATTED, dst_len - 1);
    dst[dst_len - 1] = '\0';
    return true;
}

void setUp(void) {
    Mockswap_utils_Init();
    Mockformat_Init();
}

void tearDown(void) {
    Mockswap_utils_Verify();
    Mockformat_Verify();
    Mockswap_utils_Destroy();
    Mockformat_Destroy();
}

void test_amount_too_big(void) {
    get_printable_amount_parameters_t params = {0};
    params.amount = g_amount;
    params.amount_length = sizeof(g_amount);

    swap_str_to_u64_ExpectAnyArgsAndReturn(false);  // amount conversion fails

    swap_handle_get_printable_amount(&params);

    TEST_ASSERT_EQUAL_STRING("", params.printable_amount);
}

void test_native_amount_for_fee(void) {
    get_printable_amount_parameters_t params = {0};
    params.amount = g_amount;
    params.amount_length = sizeof(g_amount);
    params.is_fee = true;  // -> native BOL, no config parsing

    swap_str_to_u64_ExpectAnyArgsAndReturn(true);
    format_fpu64_AddCallback(fpu64_cb);
    format_fpu64_ExpectAnyArgsAndReturn(true);

    swap_handle_get_printable_amount(&params);

    TEST_ASSERT_EQUAL_STRING(FORMATTED " BOL", params.printable_amount);
}

void test_native_amount_without_config(void) {
    get_printable_amount_parameters_t params = {0};
    params.amount = g_amount;
    params.amount_length = sizeof(g_amount);
    params.is_fee = false;
    params.coin_configuration = NULL;  // -> native BOL

    swap_str_to_u64_ExpectAnyArgsAndReturn(true);
    format_fpu64_AddCallback(fpu64_cb);
    format_fpu64_ExpectAnyArgsAndReturn(true);

    swap_handle_get_printable_amount(&params);

    TEST_ASSERT_EQUAL_STRING(FORMATTED " BOL", params.printable_amount);
}

void test_token_amount_with_config(void) {
    get_printable_amount_parameters_t params = {0};
    params.amount = g_amount;
    params.amount_length = sizeof(g_amount);
    params.is_fee = false;
    params.coin_configuration = g_config;
    params.coin_configuration_length = sizeof(g_config);

    swap_str_to_u64_ExpectAnyArgsAndReturn(true);
    swap_parse_config_AddCallback(parse_config_cb);
    swap_parse_config_ExpectAnyArgsAndReturn(true);
    format_fpu64_AddCallback(fpu64_cb);
    format_fpu64_ExpectAnyArgsAndReturn(true);

    swap_handle_get_printable_amount(&params);

    TEST_ASSERT_EQUAL_STRING(FORMATTED " USDC", params.printable_amount);
}

void test_config_parse_failure(void) {
    get_printable_amount_parameters_t params = {0};
    params.amount = g_amount;
    params.amount_length = sizeof(g_amount);
    params.is_fee = false;
    params.coin_configuration = g_config;
    params.coin_configuration_length = sizeof(g_config);

    swap_str_to_u64_ExpectAnyArgsAndReturn(true);
    swap_parse_config_ExpectAnyArgsAndReturn(false);  // config parsing fails

    swap_handle_get_printable_amount(&params);

    TEST_ASSERT_EQUAL_STRING("", params.printable_amount);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_amount_too_big);
    RUN_TEST(test_native_amount_for_fee);
    RUN_TEST(test_native_amount_without_config);
    RUN_TEST(test_token_amount_with_config);
    RUN_TEST(test_config_parse_failure);

    return UNITY_END();
}
