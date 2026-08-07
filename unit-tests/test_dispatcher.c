/*
 * Unit tests for src/apdu/dispatcher.c.
 *
 * apdu_dispatcher() routes a parsed APDU to the right handler after validating
 * CLA / INS / P1 / P2 / data presence. Every dependency is mocked: the command
 * handlers (one mocked header each) and io_send_sw — which is a static-inline
 * wrapper, so the mockable symbol is io_send_response_buffers
 * (io_send_sw(sw) == io_send_response_buffers(NULL, 0, sw)).
 *
 * One test per APDU (grouping its valid / invalid-P1P2 / missing-data cases):
 * the CMock expectation is queued right before each dispatch, so the sequence
 * reads top to bottom and there is no mysterious duplicated expectation.
 */

#include <setjmp.h>

#include "unity.h"

#include "dispatcher.h"
#include "constants.h"
#include "types.h"
#include "status_words.h"

#include "Mockio.h"
#include "Mockget_version.h"
#include "Mockget_app_name.h"
#include "Mockget_public_key.h"
#include "Mocksign_tx.h"
#include "Mockprovide_token_info.h"
#include "Mockledger_assert_internals.h"

// Distinctive value returned by the mocked handlers (not a status word) so we
// can assert the dispatcher forwards the handler's return value verbatim.
#define HANDLER_RET 0x4242

// Non-NULL command data for the INS that require it.
static uint8_t g_data[4] = {0xde, 0xad, 0xbe, 0xef};

void setUp(void) {
    Mockio_Init();
    Mockget_version_Init();
    Mockget_app_name_Init();
    Mockget_public_key_Init();
    Mocksign_tx_Init();
    Mockprovide_token_info_Init();
    Mockledger_assert_internals_Init();
}

void tearDown(void) {
    Mockio_Verify();
    Mockget_version_Verify();
    Mockget_app_name_Verify();
    Mockget_public_key_Verify();
    Mocksign_tx_Verify();
    Mockprovide_token_info_Verify();
    Mockledger_assert_internals_Verify();

    Mockio_Destroy();
    Mockget_version_Destroy();
    Mockget_app_name_Destroy();
    Mockget_public_key_Destroy();
    Mocksign_tx_Destroy();
    Mockprovide_token_info_Destroy();
    Mockledger_assert_internals_Destroy();
}

#define EXPECT_SEND_SW(sw) io_send_response_buffers_ExpectAndReturn(NULL, 0, (sw), (sw))

// Captures of the arguments forwarded to the get_public_key / sign_tx handlers.
static bool g_gpk_display;
static uint8_t g_sign_chunk;
static bool g_sign_more;
static bool g_sign_is_token;

static int gpk_cb(buffer_t *cdata, bool display, int cmock_num_calls) {
    (void) cdata;
    (void) cmock_num_calls;
    g_gpk_display = display;
    return HANDLER_RET;
}

static int sign_cb(buffer_t *cdata, uint8_t chunk, bool more, bool is_token, int cmock_num_calls) {
    (void) cdata;
    (void) cmock_num_calls;
    g_sign_chunk = chunk;
    g_sign_more = more;
    g_sign_is_token = is_token;
    return HANDLER_RET;
}

// =========================================================================
// NULL command -> LEDGER_ASSERT fires (assert_exit)
// =========================================================================
static jmp_buf g_assert_jmp;

static void assert_exit_cb(bool confirm, int cmock_num_calls) {
    (void) confirm;
    (void) cmock_num_calls;
    longjmp(g_assert_jmp, 1);
}

void test_invalid_command(void) {
    assert_exit_AddCallback(assert_exit_cb);
    assert_exit_ExpectAnyArgs();

    if (setjmp(g_assert_jmp) == 0) {
        apdu_dispatcher(NULL);
        TEST_FAIL_MESSAGE("apdu_dispatcher should not return on NULL cmd");
    }
    // Reached via the longjmp from assert_exit_cb: the assertion fired.
}

// =========================================================================
// Framing: wrong CLA, unknown INS
// =========================================================================

void test_framing_errors(void) {
    command_t bad_cla = {.cla = CLA - 1};
    EXPECT_SEND_SW(SWO_INVALID_CLA);
    TEST_ASSERT_EQUAL(SWO_INVALID_CLA, apdu_dispatcher(&bad_cla));

    command_t bad_ins = {.cla = CLA, .ins = 0x01};
    EXPECT_SEND_SW(SWO_INVALID_INS);
    TEST_ASSERT_EQUAL(SWO_INVALID_INS, apdu_dispatcher(&bad_ins));
}

// =========================================================================
// GET_VERSION
// =========================================================================

void test_get_version(void) {
    command_t valid = {.cla = CLA, .ins = GET_VERSION, .p1 = 0, .p2 = 0};
    handler_get_version_ExpectAndReturn(HANDLER_RET);
    TEST_ASSERT_EQUAL(HANDLER_RET, apdu_dispatcher(&valid));

    command_t bad_p1 = {.cla = CLA, .ins = GET_VERSION, .p1 = 1, .p2 = 0};
    EXPECT_SEND_SW(SWO_INCORRECT_P1_P2);
    TEST_ASSERT_EQUAL(SWO_INCORRECT_P1_P2, apdu_dispatcher(&bad_p1));

    command_t bad_p2 = {.cla = CLA, .ins = GET_VERSION, .p1 = 0, .p2 = 1};
    EXPECT_SEND_SW(SWO_INCORRECT_P1_P2);
    TEST_ASSERT_EQUAL(SWO_INCORRECT_P1_P2, apdu_dispatcher(&bad_p2));
}

// =========================================================================
// GET_APP_NAME
// =========================================================================

void test_get_app_name(void) {
    command_t valid = {.cla = CLA, .ins = GET_APP_NAME, .p1 = 0, .p2 = 0};
    handler_get_app_name_ExpectAndReturn(HANDLER_RET);
    TEST_ASSERT_EQUAL(HANDLER_RET, apdu_dispatcher(&valid));

    command_t bad_p1_p2 = {.cla = CLA, .ins = GET_APP_NAME, .p1 = 1, .p2 = 1};
    EXPECT_SEND_SW(SWO_INCORRECT_P1_P2);
    TEST_ASSERT_EQUAL(SWO_INCORRECT_P1_P2, apdu_dispatcher(&bad_p1_p2));
}

// =========================================================================
// GET_PUBLIC_KEY (display flag = p1; p2 must be 0; data required)
// =========================================================================

void test_get_public_key(void) {
    handler_get_public_key_AddCallback(gpk_cb);

    command_t no_display =
        {.cla = CLA, .ins = GET_PUBLIC_KEY, .p1 = 0, .p2 = 0, .lc = 4, .data = g_data};
    handler_get_public_key_ExpectAnyArgsAndReturn(HANDLER_RET);
    TEST_ASSERT_EQUAL(HANDLER_RET, apdu_dispatcher(&no_display));
    TEST_ASSERT_FALSE(g_gpk_display);

    command_t with_display =
        {.cla = CLA, .ins = GET_PUBLIC_KEY, .p1 = 1, .p2 = 0, .lc = 4, .data = g_data};
    handler_get_public_key_ExpectAnyArgsAndReturn(HANDLER_RET);
    TEST_ASSERT_EQUAL(HANDLER_RET, apdu_dispatcher(&with_display));
    TEST_ASSERT_TRUE(g_gpk_display);

    command_t bad_p1 =
        {.cla = CLA, .ins = GET_PUBLIC_KEY, .p1 = 2, .p2 = 0, .lc = 4, .data = g_data};
    EXPECT_SEND_SW(SWO_INCORRECT_P1_P2);
    TEST_ASSERT_EQUAL(SWO_INCORRECT_P1_P2, apdu_dispatcher(&bad_p1));

    command_t bad_p2 =
        {.cla = CLA, .ins = GET_PUBLIC_KEY, .p1 = 0, .p2 = 1, .lc = 4, .data = g_data};
    EXPECT_SEND_SW(SWO_INCORRECT_P1_P2);
    TEST_ASSERT_EQUAL(SWO_INCORRECT_P1_P2, apdu_dispatcher(&bad_p2));

    command_t no_data =
        {.cla = CLA, .ins = GET_PUBLIC_KEY, .p1 = 0, .p2 = 0, .lc = 0, .data = NULL};
    EXPECT_SEND_SW(SWO_WRONG_DATA_LENGTH);
    TEST_ASSERT_EQUAL(SWO_WRONG_DATA_LENGTH, apdu_dispatcher(&no_data));
}

// =========================================================================
// SIGN_TX (chunked: p1 = chunk index, p2 = P2_MORE/P2_LAST; data required)
// =========================================================================

void test_sign_tx(void) {
    handler_sign_tx_AddCallback(sign_cb);

    // First chunk: p1 == P1_START with P2_MORE -> more = true, not a token tx.
    command_t first =
        {.cla = CLA, .ins = SIGN_TX, .p1 = P1_START, .p2 = P2_MORE, .lc = 4, .data = g_data};
    handler_sign_tx_ExpectAnyArgsAndReturn(HANDLER_RET);
    TEST_ASSERT_EQUAL(HANDLER_RET, apdu_dispatcher(&first));
    TEST_ASSERT_EQUAL(P1_START, g_sign_chunk);
    TEST_ASSERT_TRUE(g_sign_more);
    TEST_ASSERT_FALSE(g_sign_is_token);

    // Last chunk: p1 = 1 with P2_LAST -> more = false.
    command_t last = {.cla = CLA, .ins = SIGN_TX, .p1 = 1, .p2 = P2_LAST, .lc = 4, .data = g_data};
    handler_sign_tx_ExpectAnyArgsAndReturn(HANDLER_RET);
    TEST_ASSERT_EQUAL(HANDLER_RET, apdu_dispatcher(&last));
    TEST_ASSERT_EQUAL(1, g_sign_chunk);
    TEST_ASSERT_FALSE(g_sign_more);

    // p1 == P1_START but not P2_MORE -> rejected.
    command_t start_not_more =
        {.cla = CLA, .ins = SIGN_TX, .p1 = P1_START, .p2 = P2_LAST, .lc = 4, .data = g_data};
    EXPECT_SEND_SW(SWO_INCORRECT_P1_P2);
    TEST_ASSERT_EQUAL(SWO_INCORRECT_P1_P2, apdu_dispatcher(&start_not_more));

    // p1 > P1_MAX -> rejected.
    command_t p1_too_big =
        {.cla = CLA, .ins = SIGN_TX, .p1 = P1_MAX + 1, .p2 = P2_MORE, .lc = 4, .data = g_data};
    EXPECT_SEND_SW(SWO_INCORRECT_P1_P2);
    TEST_ASSERT_EQUAL(SWO_INCORRECT_P1_P2, apdu_dispatcher(&p1_too_big));

    // p2 neither P2_LAST nor P2_MORE -> rejected.
    command_t bad_p2 = {.cla = CLA, .ins = SIGN_TX, .p1 = 1, .p2 = 0x01, .lc = 4, .data = g_data};
    EXPECT_SEND_SW(SWO_INCORRECT_P1_P2);
    TEST_ASSERT_EQUAL(SWO_INCORRECT_P1_P2, apdu_dispatcher(&bad_p2));

    // Missing data.
    command_t no_data =
        {.cla = CLA, .ins = SIGN_TX, .p1 = P1_START, .p2 = P2_MORE, .lc = 0, .data = NULL};
    EXPECT_SEND_SW(SWO_WRONG_DATA_LENGTH);
    TEST_ASSERT_EQUAL(SWO_WRONG_DATA_LENGTH, apdu_dispatcher(&no_data));
}

// =========================================================================
// SIGN_TOKEN_TX (same handler as SIGN_TX, with is_token = true)
// =========================================================================

void test_sign_token_tx(void) {
    handler_sign_tx_AddCallback(sign_cb);

    command_t token =
        {.cla = CLA, .ins = SIGN_TOKEN_TX, .p1 = P1_START, .p2 = P2_MORE, .lc = 4, .data = g_data};
    handler_sign_tx_ExpectAnyArgsAndReturn(HANDLER_RET);
    TEST_ASSERT_EQUAL(HANDLER_RET, apdu_dispatcher(&token));
    TEST_ASSERT_TRUE(g_sign_is_token);
}

// =========================================================================
// PROVIDE_TOKEN_INFO
// =========================================================================

void test_provide_token_info(void) {
    command_t valid =
        {.cla = CLA, .ins = PROVIDE_TOKEN_INFO, .p1 = 0, .p2 = 0, .lc = 4, .data = g_data};
    handler_provide_token_info_ExpectAnyArgsAndReturn(HANDLER_RET);
    TEST_ASSERT_EQUAL(HANDLER_RET, apdu_dispatcher(&valid));

    command_t bad_p1 =
        {.cla = CLA, .ins = PROVIDE_TOKEN_INFO, .p1 = 1, .p2 = 0, .lc = 4, .data = g_data};
    EXPECT_SEND_SW(SWO_INCORRECT_P1_P2);
    TEST_ASSERT_EQUAL(SWO_INCORRECT_P1_P2, apdu_dispatcher(&bad_p1));

    command_t bad_p2 =
        {.cla = CLA, .ins = PROVIDE_TOKEN_INFO, .p1 = 0, .p2 = 1, .lc = 4, .data = g_data};
    EXPECT_SEND_SW(SWO_INCORRECT_P1_P2);
    TEST_ASSERT_EQUAL(SWO_INCORRECT_P1_P2, apdu_dispatcher(&bad_p2));

    command_t no_data =
        {.cla = CLA, .ins = PROVIDE_TOKEN_INFO, .p1 = 0, .p2 = 0, .lc = 0, .data = NULL};
    EXPECT_SEND_SW(SWO_WRONG_DATA_LENGTH);
    TEST_ASSERT_EQUAL(SWO_WRONG_DATA_LENGTH, apdu_dispatcher(&no_data));
}

// =========================================================================

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_invalid_command);
    RUN_TEST(test_framing_errors);
    RUN_TEST(test_get_version);
    RUN_TEST(test_get_app_name);
    RUN_TEST(test_get_public_key);
    RUN_TEST(test_sign_tx);
    RUN_TEST(test_sign_token_tx);
    RUN_TEST(test_provide_token_info);

    return UNITY_END();
}
