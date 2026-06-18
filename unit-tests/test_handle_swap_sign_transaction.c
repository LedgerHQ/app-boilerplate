/*
 * Unit tests for src/swap/handle_swap_sign_transaction.c.
 *
 * Two entry points:
 *   - swap_copy_transaction_parameters(): validate + stash the swap parameters
 *     into G_swap_validated;
 *   - swap_check_validity(): compare the to-be-signed tx against the stashed
 *     parameters, exiting (via send_swap_error_simple, noreturn) on any mismatch.
 *
 * Every dependency is a CMock mock (no real dependency code is linked):
 *   - swap_str_to_u64 / swap_parse_config   (swap_utils.h)
 *   - format_hex                            (format.h)
 *   - send_swap_error_simple                (swap_error_code_helpers.h)
 *   - os_sched_exit                         (os_task.h)
 *   - os_explicit_zero_BSS_segment          (os_utils.h)
 * The two noreturn syscalls get an AddCallback that longjmps, so they honour
 * their "does not return" contract instead of falling through.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <setjmp.h>

#include "unity.h"

#include "Mockswap_utils.h"
#include "Mockformat.h"
#include "Mockswap_error_code_helpers.h"
#include "Mockos_task.h"
#include "Mockos_utils.h"

#include "swap.h"        // create_transaction_parameters_t, swap_copy_transaction_parameters
#include "handle_swap.h"  // swap_check_validity (app-specific, not in swap.h)
#include "globals.h"     // G_context
#include "types.h"       // token_info_t
#include "tx_types.h"    // ADDRESS_LEN
#include "constants.h"   // EXPONENT_SMALLEST_UNIT

global_ctx_t G_context;

// 40 hex chars (digits only -> uppercase-invariant, matches what the SUT stores).
#define RECIPIENT "0011223344556677889900112233445566778899"
#define SWAP_AMOUNT 1000ULL
#define SWAP_FEE 10ULL

// ---- noreturn exits routed back to the test ----
static jmp_buf g_exit_jmp;
static bool g_error_called;
static uint16_t g_error_sw;

// send_swap_error_simple is noreturn in the SUT; the source always follows it
// with os_sched_exit(0). We let the mock RETURN (just record) so that trailing
// os_sched_exit line actually executes, and route the longjmp from os_sched_exit
// instead -> both lines get covered.
static void send_swap_error_cb(uint16_t sw, uint8_t c, uint8_t a, int cmock_num_calls) {
    (void) c;
    (void) a;
    (void) cmock_num_calls;
    g_error_called = true;
    g_error_sw = sw;
}

static void os_sched_exit_cb(bolos_task_status_t code, int cmock_num_calls) {
    (void) code;
    (void) cmock_num_calls;
    longjmp(g_exit_jmp, 1);
}

static void bss_segment_cb(int cmock_num_calls) {
    (void) cmock_num_calls;  // no-op on host
}

// swap_str_to_u64: 1st call (amount) -> SWAP_AMOUNT, 2nd (fee) -> SWAP_FEE.
static bool str_to_u64_cb(const uint8_t *src, size_t length, uint64_t *result, int cmock_num_calls) {
    (void) src;
    (void) length;
    *result = (cmock_num_calls % 2 == 0) ? SWAP_AMOUNT : SWAP_FEE;
    return true;
}

// format_hex: always produce RECIPIENT.
static int format_hex_cb(const uint8_t *in, size_t in_len, char *out, size_t out_len, int n) {
    (void) in;
    (void) in_len;
    (void) n;
    strncpy(out, RECIPIENT, out_len - 1);
    out[out_len - 1] = '\0';
    return (int) strlen(out);
}

// format_hex variant producing a different address (for the mismatch case).
static int format_hex_cb_mismatch(const uint8_t *in, size_t in_len, char *out, size_t out_len, int n) {
    (void) in;
    (void) in_len;
    (void) n;
    strncpy(out, "ffffffffffffffffffffffffffffffffffffffff", out_len - 1);
    out[out_len - 1] = '\0';
    return (int) strlen(out);
}

void setUp(void) {
    Mockswap_utils_Init();
    Mockformat_Init();
    Mockswap_error_code_helpers_Init();
    Mockos_task_Init();
    Mockos_utils_Init();

    // These three have uniform behaviour across tests -> stub once here.
    send_swap_error_simple_Stub(send_swap_error_cb);
    os_sched_exit_Stub(os_sched_exit_cb);
    os_explicit_zero_BSS_segment_Stub(bss_segment_cb);

    memset(&G_context, 0, sizeof(G_context));  // is_token_tx = false
    g_error_called = false;
    g_error_sw = 0;
}

void tearDown(void) {
    Mockswap_utils_Verify();
    Mockformat_Verify();
    Mockswap_error_code_helpers_Verify();
    Mockos_task_Verify();
    Mockos_utils_Verify();

    Mockswap_utils_Destroy();
    Mockformat_Destroy();
    Mockswap_error_code_helpers_Destroy();
    Mockos_task_Destroy();
    Mockos_utils_Destroy();
}

static create_transaction_parameters_t make_create_params(char *dest) {
    static uint8_t amount[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xe8};
    static uint8_t fee[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a};
    create_transaction_parameters_t params = {0};
    params.destination_address = dest;
    params.amount = amount;
    params.amount_length = sizeof(amount);
    params.fee_amount = fee;
    params.fee_amount_length = sizeof(fee);
    params.coin_configuration = NULL;  // -> native BOL
    return params;
}

// Populate G_swap_validated with a known, valid native-BOL swap.
static void stash_valid_swap(void) {
    char dest[] = RECIPIENT;
    create_transaction_parameters_t params = make_create_params(dest);

    swap_str_to_u64_AddCallback(str_to_u64_cb);
    swap_str_to_u64_ExpectAnyArgsAndReturn(true);  // amount
    swap_str_to_u64_ExpectAnyArgsAndReturn(true);  // fee

    TEST_ASSERT_TRUE(swap_copy_transaction_parameters(&params));
}

// swap_parse_config mock: a non-BOL token ("TKN", 6 decimals) so is_token_swap()
// is true for the stashed swap.
static uint8_t g_coin_config[4] = {0xaa, 0xbb, 0xcc, 0xdd};

static bool parse_config_cb(const uint8_t *config,
                            uint8_t config_len,
                            char *ticker,
                            uint8_t ticker_buf_len,
                            uint8_t *decimals,
                            int cmock_num_calls) {
    (void) config;
    (void) config_len;
    (void) cmock_num_calls;
    strncpy(ticker, "TKN", ticker_buf_len - 1);
    ticker[ticker_buf_len - 1] = '\0';
    *decimals = 6;
    return true;
}

// Populate G_swap_validated with a token swap (TKN / 6 decimals).
static void stash_token_swap(void) {
    char dest[] = RECIPIENT;
    create_transaction_parameters_t params = make_create_params(dest);
    params.coin_configuration = g_coin_config;
    params.coin_configuration_length = sizeof(g_coin_config);

    swap_parse_config_AddCallback(parse_config_cb);
    swap_parse_config_ExpectAnyArgsAndReturn(true);
    swap_str_to_u64_AddCallback(str_to_u64_cb);
    swap_str_to_u64_ExpectAnyArgsAndReturn(true);  // amount
    swap_str_to_u64_ExpectAnyArgsAndReturn(true);  // fee

    TEST_ASSERT_TRUE(swap_copy_transaction_parameters(&params));
}

// =========================================================================
// swap_check_validity: uninitialised MUST be first (G_swap_validated is a
// static that persists across tests; stash_valid_swap() would set it).
// =========================================================================

void test_check_validity_uninitialized(void) {
    uint8_t dest[ADDRESS_LEN] = {0};
    token_info_t token = {0};

    if (setjmp(g_exit_jmp) == 0) {
        swap_check_validity(SWAP_AMOUNT, SWAP_FEE, dest, &token);
        TEST_FAIL_MESSAGE("expected swap error exit on uninitialised state");
    }
    TEST_ASSERT_TRUE(g_error_called);
}

// =========================================================================
// swap_copy_transaction_parameters
// =========================================================================

void test_copy_null_destination(void) {
    create_transaction_parameters_t params = make_create_params(NULL);
    TEST_ASSERT_FALSE(swap_copy_transaction_parameters(&params));
}

void test_copy_wrong_destination_length(void) {
    char dest[] = "deadbeef";  // != ADDRESS_LEN * 2
    create_transaction_parameters_t params = make_create_params(dest);
    TEST_ASSERT_FALSE(swap_copy_transaction_parameters(&params));
}

void test_copy_null_amount(void) {
    char dest[] = RECIPIENT;
    create_transaction_parameters_t params = make_create_params(dest);
    params.amount = NULL;
    TEST_ASSERT_FALSE(swap_copy_transaction_parameters(&params));
}

void test_copy_amount_conversion_failure(void) {
    char dest[] = RECIPIENT;
    create_transaction_parameters_t params = make_create_params(dest);

    swap_str_to_u64_ExpectAnyArgsAndReturn(false);  // amount conversion fails

    TEST_ASSERT_FALSE(swap_copy_transaction_parameters(&params));
}

void test_copy_fee_conversion_failure(void) {
    char dest[] = RECIPIENT;
    create_transaction_parameters_t params = make_create_params(dest);

    swap_str_to_u64_ExpectAnyArgsAndReturn(true);   // amount ok
    swap_str_to_u64_ExpectAnyArgsAndReturn(false);  // fee fails

    TEST_ASSERT_FALSE(swap_copy_transaction_parameters(&params));
}

void test_copy_success(void) {
    char dest[] = RECIPIENT;
    create_transaction_parameters_t params = make_create_params(dest);

    swap_str_to_u64_AddCallback(str_to_u64_cb);
    swap_str_to_u64_ExpectAnyArgsAndReturn(true);
    swap_str_to_u64_ExpectAnyArgsAndReturn(true);

    TEST_ASSERT_TRUE(swap_copy_transaction_parameters(&params));
}

void test_copy_uppercases_recipient(void) {
    // A destination with lowercase hex letters exercises the a-z -> A-Z branch.
    char dest[] = "abcdef00112233445566778899aabbccddeeff00";
    create_transaction_parameters_t params = make_create_params(dest);

    swap_str_to_u64_AddCallback(str_to_u64_cb);
    swap_str_to_u64_ExpectAnyArgsAndReturn(true);
    swap_str_to_u64_ExpectAnyArgsAndReturn(true);

    TEST_ASSERT_TRUE(swap_copy_transaction_parameters(&params));
}

void test_copy_config_parse_failure(void) {
    char dest[] = RECIPIENT;
    create_transaction_parameters_t params = make_create_params(dest);
    params.coin_configuration = g_coin_config;
    params.coin_configuration_length = sizeof(g_coin_config);

    swap_parse_config_ExpectAnyArgsAndReturn(false);  // coin config parsing fails

    TEST_ASSERT_FALSE(swap_copy_transaction_parameters(&params));
}

// =========================================================================
// swap_check_validity (native BOL, non-token)
// =========================================================================

void test_check_validity_success(void) {
    stash_valid_swap();

    uint8_t dest[ADDRESS_LEN] = {0};
    token_info_t token = {0};

    format_hex_AddCallback(format_hex_cb);
    format_hex_ExpectAnyArgsAndReturn(ADDRESS_LEN * 2);

    bool ok;
    if (setjmp(g_exit_jmp) == 0) {
        ok = swap_check_validity(SWAP_AMOUNT, SWAP_FEE, dest, &token);
    } else {
        TEST_FAIL_MESSAGE("unexpected swap error exit on valid tx");
        return;
    }
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_FALSE(g_error_called);
}

void test_check_validity_amount_mismatch(void) {
    stash_valid_swap();

    uint8_t dest[ADDRESS_LEN] = {0};
    token_info_t token = {0};

    if (setjmp(g_exit_jmp) == 0) {
        swap_check_validity(SWAP_AMOUNT + 1, SWAP_FEE, dest, &token);  // wrong amount
        TEST_FAIL_MESSAGE("expected swap error exit on amount mismatch");
    }
    TEST_ASSERT_TRUE(g_error_called);
}

void test_check_validity_fee_mismatch(void) {
    stash_valid_swap();

    uint8_t dest[ADDRESS_LEN] = {0};
    token_info_t token = {0};

    if (setjmp(g_exit_jmp) == 0) {
        swap_check_validity(SWAP_AMOUNT, SWAP_FEE + 1, dest, &token);  // wrong fee
        TEST_FAIL_MESSAGE("expected swap error exit on fee mismatch");
    }
    TEST_ASSERT_TRUE(g_error_called);
}

void test_check_validity_destination_mismatch(void) {
    stash_valid_swap();

    uint8_t dest[ADDRESS_LEN] = {0};
    token_info_t token = {0};

    // format_hex yields a different address than the stashed recipient.
    format_hex_AddCallback(format_hex_cb_mismatch);
    format_hex_ExpectAnyArgsAndReturn(ADDRESS_LEN * 2);
    if (setjmp(g_exit_jmp) == 0) {
        swap_check_validity(SWAP_AMOUNT, SWAP_FEE, dest, &token);
        TEST_FAIL_MESSAGE("expected swap error exit on destination mismatch");
    }
    TEST_ASSERT_TRUE(g_error_called);
}

// =========================================================================
// swap_check_validity: token branches (is_token_tx / is_token_swap)
// =========================================================================

void test_check_validity_token_match(void) {
    stash_token_swap();  // G_swap_validated = TKN / 6
    G_context.tx_info.is_token_tx = true;

    uint8_t dest[ADDRESS_LEN] = {0};
    token_info_t token = {.ticker = "TKN", .decimals = 6};

    format_hex_AddCallback(format_hex_cb);
    format_hex_ExpectAnyArgsAndReturn(ADDRESS_LEN * 2);

    bool ok;
    if (setjmp(g_exit_jmp) == 0) {
        ok = swap_check_validity(SWAP_AMOUNT, SWAP_FEE, dest, &token);
    } else {
        TEST_FAIL_MESSAGE("unexpected swap error exit on matching token");
        return;
    }
    TEST_ASSERT_TRUE(ok);
}

void test_check_validity_token_mismatch(void) {
    stash_token_swap();
    G_context.tx_info.is_token_tx = true;

    uint8_t dest[ADDRESS_LEN] = {0};
    token_info_t token = {.ticker = "OTHER", .decimals = 9};

    if (setjmp(g_exit_jmp) == 0) {
        swap_check_validity(SWAP_AMOUNT, SWAP_FEE, dest, &token);
        TEST_FAIL_MESSAGE("expected swap error exit on token mismatch");
    }
    TEST_ASSERT_TRUE(g_error_called);
}

void test_check_validity_unexpected_token(void) {
    stash_valid_swap();  // BOL -> is_token_swap() == false
    G_context.tx_info.is_token_tx = true;

    uint8_t dest[ADDRESS_LEN] = {0};
    token_info_t token = {0};

    if (setjmp(g_exit_jmp) == 0) {
        swap_check_validity(SWAP_AMOUNT, SWAP_FEE, dest, &token);
        TEST_FAIL_MESSAGE("expected swap error exit on unexpected token tx");
    }
    TEST_ASSERT_TRUE(g_error_called);
}

void test_check_validity_token_expected(void) {
    stash_token_swap();  // token -> is_token_swap() == true
    G_context.tx_info.is_token_tx = false;

    uint8_t dest[ADDRESS_LEN] = {0};
    token_info_t token = {0};

    if (setjmp(g_exit_jmp) == 0) {
        swap_check_validity(SWAP_AMOUNT, SWAP_FEE, dest, &token);
        TEST_FAIL_MESSAGE("expected swap error exit when token tx expected");
    }
    TEST_ASSERT_TRUE(g_error_called);
}

int main(void) {
    UNITY_BEGIN();

    // Must run before any successful stash (static G_swap_validated persists).
    RUN_TEST(test_check_validity_uninitialized);

    RUN_TEST(test_copy_null_destination);
    RUN_TEST(test_copy_wrong_destination_length);
    RUN_TEST(test_copy_null_amount);
    RUN_TEST(test_copy_amount_conversion_failure);
    RUN_TEST(test_copy_fee_conversion_failure);
    RUN_TEST(test_copy_success);
    RUN_TEST(test_copy_uppercases_recipient);
    RUN_TEST(test_copy_config_parse_failure);

    RUN_TEST(test_check_validity_success);
    RUN_TEST(test_check_validity_amount_mismatch);
    RUN_TEST(test_check_validity_fee_mismatch);
    RUN_TEST(test_check_validity_destination_mismatch);

    RUN_TEST(test_check_validity_token_match);
    RUN_TEST(test_check_validity_token_mismatch);
    RUN_TEST(test_check_validity_unexpected_token);
    RUN_TEST(test_check_validity_token_expected);

    return UNITY_END();
}
