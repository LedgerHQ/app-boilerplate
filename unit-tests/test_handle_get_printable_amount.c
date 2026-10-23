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
static bool fpu64_cb(char *dst,
                     size_t dst_len,
                     uint64_t value,
                     uint8_t decimals,
                     int cmock_num_calls) {
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

// format_fpu64 mock: write FORMATTED, then report a failure.
static bool fpu64_fail_cb(char *dst,
                          size_t dst_len,
                          uint64_t value,
                          uint8_t decimals,
                          int cmock_num_calls) {
    fpu64_cb(dst, dst_len, value, decimals, cmock_num_calls);
    return false;
}

void test_format_failure(void) {
    get_printable_amount_parameters_t params = {0};
    params.amount = g_amount;
    params.amount_length = sizeof(g_amount);
    params.is_fee = true;

    swap_str_to_u64_ExpectAnyArgsAndReturn(true);
    format_fpu64_AddCallback(fpu64_fail_cb);
    format_fpu64_ExpectAnyArgsAndReturn(false);  // formatting fails

    swap_handle_get_printable_amount(&params);

    TEST_ASSERT_EQUAL_STRING("", params.printable_amount);
}

// Ticker length produced by parse_sized_ticker_cb.
static size_t g_ticker_len;

// swap_parse_config mock: yield a ticker of g_ticker_len characters.
static bool parse_sized_ticker_cb(const uint8_t *config,
                                  uint8_t config_len,
                                  char *ticker,
                                  uint8_t ticker_buf_len,
                                  uint8_t *decimals,
                                  int cmock_num_calls) {
    (void) config;
    (void) config_len;
    (void) cmock_num_calls;
    TEST_ASSERT_TRUE(g_ticker_len < ticker_buf_len);
    memset(ticker, 'A', g_ticker_len);
    ticker[g_ticker_len] = '\0';
    *decimals = 6;
    return true;
}

static void run_with_ticker_len(get_printable_amount_parameters_t *params, size_t ticker_len) {
    params->amount = g_amount;
    params->amount_length = sizeof(g_amount);
    params->is_fee = false;
    params->coin_configuration = g_config;
    params->coin_configuration_length = sizeof(g_config);
    g_ticker_len = ticker_len;

    swap_str_to_u64_ExpectAnyArgsAndReturn(true);
    swap_parse_config_AddCallback(parse_sized_ticker_cb);
    swap_parse_config_ExpectAnyArgsAndReturn(true);
    format_fpu64_AddCallback(fpu64_cb);
    format_fpu64_ExpectAnyArgsAndReturn(true);

    swap_handle_get_printable_amount(params);
}

// Longest ticker such that "<FORMATTED> <ticker>" and its terminator fill the buffer.
#define MAX_FITTING_TICKER_LEN (MAX_PRINTABLE_AMOUNT_SIZE - sizeof(FORMATTED " "))

void test_printable_amount_max_length(void) {
    get_printable_amount_parameters_t params = {0};

    run_with_ticker_len(&params, MAX_FITTING_TICKER_LEN);

    TEST_ASSERT_EQUAL_size_t(MAX_PRINTABLE_AMOUNT_SIZE - 1, strlen(params.printable_amount));
    TEST_ASSERT_EQUAL_STRING_LEN(FORMATTED " AAA",
                                 params.printable_amount,
                                 sizeof(FORMATTED " AAA") - 1);
}

void test_printable_amount_too_long(void) {
    get_printable_amount_parameters_t params = {0};

    run_with_ticker_len(&params, MAX_FITTING_TICKER_LEN + 1);

    TEST_ASSERT_EQUAL_STRING("", params.printable_amount);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_amount_too_big);
    RUN_TEST(test_native_amount_for_fee);
    RUN_TEST(test_native_amount_without_config);
    RUN_TEST(test_token_amount_with_config);
    RUN_TEST(test_config_parse_failure);
    RUN_TEST(test_format_failure);
    RUN_TEST(test_printable_amount_max_length);
    RUN_TEST(test_printable_amount_too_long);

    return UNITY_END();
}
