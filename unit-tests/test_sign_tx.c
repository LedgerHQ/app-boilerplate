/*
 * Unit tests for src/handler/sign_tx.c (HAVE_SWAP build).
 *
 * handler_sign_tx() accumulates APDU chunks then parses, hashes and routes the
 * transaction to the right review screen (or to the swap bypass). Every
 * dependency is a CMock mock (set up as a _Stub driven by control globals):
 *   buffer (read_u8 / read_bip32_path / move), io_send_response_buffers,
 *   transaction_deserialize, get_token_info, cx_keccak_256_hash_iovec,
 *   validate_transaction, ui_display_{transaction,blind_signed,token},
 *   swap_check_validity, send_swap_error_simple, os_sched_exit.
 * The noreturn exits (send_swap_error_simple / os_sched_exit) longjmp back.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <setjmp.h>

#include "unity.h"

#include "Mockbuffer.h"
#include "Mockio.h"
#include "Mockdeserialize.h"
#include "Mocklcx_sha3.h"
#include "Mockvalidate.h"
#include "Mockdisplay.h"
#include "Mockhandle_swap.h"
#include "Mockswap_error_code_helpers.h"
#include "Mockos_task.h"

#include "sign_tx.h"
#include "dynamic_token_info.h"  // get_token_info (CMock can't mock this header:
                                 // set_token_info has a char(*)[N] param it
                                 // fails to parse -> hand-stub get_token_info)
#include "globals.h"
#include "types.h"
#include "sw.h"

global_ctx_t G_context;
volatile bool G_called_from_swap;
volatile bool G_swap_response_ready;

// ---- control knobs / captures ----
static bool g_read_u8_ret, g_read_path_ret, g_buffer_move_ret;
static parser_status_e g_deser_status;
static const char *g_deser_memo;
static bool g_get_token_info_ret;
static cx_err_t g_keccak_ret;
static bool g_swap_check_ret;

static uint16_t g_last_sw;
static bool g_validate_called, g_validate_choice;
static bool g_swap_error_called;
static enum { DISP_NONE, DISP_STD, DISP_BLIND, DISP_TOKEN } g_display;

static jmp_buf g_exit_jmp;

// ---- mock callbacks ----
static bool read_u8_cb(buffer_t *b, uint8_t *v, int n) {
    (void) b; (void) n;
    *v = 5;  // a plausible bip32 path length
    return g_read_u8_ret;
}
static bool read_path_cb(buffer_t *b, uint32_t *o, size_t l, int n) {
    (void) b; (void) o; (void) l; (void) n;
    return g_read_path_ret;
}
static bool buffer_move_cb(buffer_t *b, uint8_t *o, size_t l, int n) {
    (void) b; (void) o; (void) l; (void) n;
    return g_buffer_move_ret;
}
static parser_status_e deser_cb(buffer_t *b, transaction_t *tx, bool is_token, int n) {
    (void) b; (void) is_token; (void) n;
    tx->memo = (uint8_t *) g_deser_memo;
    return g_deser_status;
}
// Hand-stub (dynamic_token_info.h is not CMock-mockable).
bool get_token_info(const uint8_t *token_address, token_info_t *token_info) {
    (void) token_address;
    (void) token_info;
    return g_get_token_info_ret;
}
static cx_err_t keccak_cb(const cx_iovec_t *iv, size_t len, uint8_t *digest, int n) {
    (void) iv; (void) len; (void) digest; (void) n;
    return g_keccak_ret;
}
static void validate_cb(bool choice, int n) {
    (void) n;
    g_validate_called = true;
    g_validate_choice = choice;
}
static bool swap_check_cb(uint64_t a, uint64_t f, const uint8_t *d, const token_info_t *t, int n) {
    (void) a; (void) f; (void) d; (void) t; (void) n;
    return g_swap_check_ret;
}
static int io_send_cb(const buffer_t *r, size_t c, uint16_t sw, int n) {
    (void) r; (void) c; (void) n;
    g_last_sw = sw;
    return (int) sw;
}
static int disp_std_cb(int n) { (void) n; g_display = DISP_STD; return 0; }
static int disp_blind_cb(int n) { (void) n; g_display = DISP_BLIND; return 0; }
static int disp_token_cb(int n) { (void) n; g_display = DISP_TOKEN; return 0; }

static void swap_error_cb(uint16_t sw, uint8_t c, uint8_t a, int n) {
    (void) sw; (void) c; (void) a; (void) n;
    g_swap_error_called = true;
    longjmp(g_exit_jmp, 1);
}
static bool g_sched_exit_longjmp;
static void sched_exit_cb(bolos_task_status_t code, int n) {
    (void) code; (void) n;
    // When allowed to "return", execution flows through the (normally
    // unreachable) `return 0;` lines after os_sched_exit -> covers them.
    if (g_sched_exit_longjmp) {
        longjmp(g_exit_jmp, 2);
    }
}

void setUp(void) {
    Mockbuffer_Init();
    Mockio_Init();
    Mockdeserialize_Init();
    Mocklcx_sha3_Init();
    Mockvalidate_Init();
    Mockdisplay_Init();
    Mockhandle_swap_Init();
    Mockswap_error_code_helpers_Init();
    Mockos_task_Init();

    buffer_read_u8_Stub(read_u8_cb);
    buffer_read_bip32_path_Stub(read_path_cb);
    buffer_move_Stub(buffer_move_cb);
    transaction_deserialize_Stub(deser_cb);
    cx_keccak_256_hash_iovec_Stub(keccak_cb);
    validate_transaction_Stub(validate_cb);
    swap_check_validity_Stub(swap_check_cb);
    io_send_response_buffers_Stub(io_send_cb);
    ui_display_transaction_Stub(disp_std_cb);
    ui_display_blind_signed_transaction_Stub(disp_blind_cb);
    ui_display_token_transaction_Stub(disp_token_cb);
    send_swap_error_simple_Stub(swap_error_cb);
    os_sched_exit_Stub(sched_exit_cb);

    memset(&G_context, 0, sizeof(G_context));
    G_called_from_swap = false;
    G_swap_response_ready = false;

    g_read_u8_ret = true;
    g_read_path_ret = true;
    g_buffer_move_ret = true;
    g_deser_status = PARSING_OK;
    g_deser_memo = "hello";
    g_get_token_info_ret = true;
    g_keccak_ret = CX_OK;
    g_swap_check_ret = true;

    g_last_sw = 0;
    g_validate_called = false;
    g_validate_choice = false;
    g_swap_error_called = false;
    g_display = DISP_NONE;
    g_sched_exit_longjmp = true;  // default: stop at os_sched_exit (overridden below)
}

void tearDown(void) {
    Mockbuffer_Verify();
    Mockio_Verify();
    Mockdeserialize_Verify();
    Mocklcx_sha3_Verify();
    Mockvalidate_Verify();
    Mockdisplay_Verify();
    Mockhandle_swap_Verify();
    Mockswap_error_code_helpers_Verify();
    Mockos_task_Verify();

    Mockbuffer_Destroy();
    Mockio_Destroy();
    Mockdeserialize_Destroy();
    Mocklcx_sha3_Destroy();
    Mockvalidate_Destroy();
    Mockdisplay_Destroy();
    Mockhandle_swap_Destroy();
    Mockswap_error_code_helpers_Destroy();
    Mockos_task_Destroy();
}

static buffer_t make_cdata(void) {
    static uint8_t bytes[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    buffer_t b = {.ptr = bytes, .size = sizeof(bytes), .offset = 0};
    return b;
}

// =========================================================================
// chunk 0: init transaction context
// =========================================================================

void test_chunk0_success(void) {
    buffer_t cdata = make_cdata();
    TEST_ASSERT_EQUAL(SWO_SUCCESS, handler_sign_tx(&cdata, 0, true, false));
}

void test_chunk0_bad_path_len(void) {
    buffer_t cdata = make_cdata();
    g_read_u8_ret = false;
    TEST_ASSERT_EQUAL(SWO_WRONG_DATA_LENGTH, handler_sign_tx(&cdata, 0, true, false));
}

void test_chunk0_bad_path(void) {
    buffer_t cdata = make_cdata();
    g_read_path_ret = false;
    TEST_ASSERT_EQUAL(SWO_WRONG_DATA_LENGTH, handler_sign_tx(&cdata, 0, true, false));
}

// =========================================================================
// chunk > 0: accumulation
// =========================================================================

void test_accumulate_wrong_req_type(void) {
    buffer_t cdata = make_cdata();
    G_context.req_type = CONFIRM_ADDRESS;  // expected CONFIRM_TRANSACTION
    TEST_ASSERT_EQUAL(SWO_CONDITIONS_NOT_SATISFIED, handler_sign_tx(&cdata, 1, true, false));
}

void test_accumulate_overflow(void) {
    buffer_t cdata = make_cdata();
    G_context.req_type = CONFIRM_TRANSACTION;
    G_context.tx_info.raw_tx_len = sizeof(G_context.tx_info.raw_tx);  // no room left
    TEST_ASSERT_EQUAL(SWO_WRONG_DATA_LENGTH, handler_sign_tx(&cdata, 1, true, false));
}

void test_accumulate_buffer_move_failure(void) {
    buffer_t cdata = make_cdata();
    G_context.req_type = CONFIRM_TRANSACTION;
    g_buffer_move_ret = false;
    TEST_ASSERT_EQUAL(SWO_INCORRECT_DATA, handler_sign_tx(&cdata, 1, true, false));
}

void test_accumulate_more_chunks(void) {
    buffer_t cdata = make_cdata();
    G_context.req_type = CONFIRM_TRANSACTION;
    TEST_ASSERT_EQUAL(SWO_SUCCESS, handler_sign_tx(&cdata, 1, true, false));  // more = true
}

// =========================================================================
// last chunk: process_transaction failures
// =========================================================================

void test_process_deserialize_failure(void) {
    buffer_t cdata = make_cdata();
    G_context.req_type = CONFIRM_TRANSACTION;
    g_deser_status = WRONG_LENGTH_ERROR;
    TEST_ASSERT_EQUAL(SWO_INCORRECT_DATA, handler_sign_tx(&cdata, 1, false, false));
}

void test_process_token_not_found(void) {
    buffer_t cdata = make_cdata();
    G_context.req_type = CONFIRM_TOKEN_TRANSACTION;
    g_get_token_info_ret = false;
    TEST_ASSERT_EQUAL(SWO_INCORRECT_DATA, handler_sign_tx(&cdata, 1, false, true));
}

void test_process_hash_failure(void) {
    buffer_t cdata = make_cdata();
    G_context.req_type = CONFIRM_TRANSACTION;
    g_keccak_ret = (cx_err_t) 1;  // != CX_OK
    TEST_ASSERT_EQUAL(SWO_INCORRECT_DATA, handler_sign_tx(&cdata, 1, false, false));
}

// =========================================================================
// last chunk: review screen routing
// =========================================================================

void test_display_standard(void) {
    buffer_t cdata = make_cdata();
    G_context.req_type = CONFIRM_TRANSACTION;
    g_deser_memo = "hello";
    handler_sign_tx(&cdata, 1, false, false);
    TEST_ASSERT_EQUAL(DISP_STD, g_display);
}

void test_display_blind_sign(void) {
    buffer_t cdata = make_cdata();
    G_context.req_type = CONFIRM_TRANSACTION;
    g_deser_memo = "Blind-sign";
    handler_sign_tx(&cdata, 1, false, false);
    TEST_ASSERT_EQUAL(DISP_BLIND, g_display);
}

void test_display_token(void) {
    buffer_t cdata = make_cdata();
    G_context.req_type = CONFIRM_TOKEN_TRANSACTION;
    g_deser_memo = "hello";
    handler_sign_tx(&cdata, 1, false, true);
    TEST_ASSERT_EQUAL(DISP_TOKEN, g_display);
}

// =========================================================================
// swap context
// =========================================================================

void test_swap_process_error_sends_swap_error(void) {
    buffer_t cdata = make_cdata();
    G_context.req_type = CONFIRM_TRANSACTION;
    G_called_from_swap = true;
    g_deser_status = WRONG_LENGTH_ERROR;  // process fails

    if (setjmp(g_exit_jmp) == 0) {
        handler_sign_tx(&cdata, 1, false, false);
        TEST_FAIL_MESSAGE("expected send_swap_error_simple exit");
    }
    TEST_ASSERT_TRUE(g_swap_error_called);
}

void test_swap_valid_signs_and_exits(void) {
    buffer_t cdata = make_cdata();
    G_context.req_type = CONFIRM_TRANSACTION;
    G_called_from_swap = true;
    G_swap_response_ready = false;
    g_swap_check_ret = true;
    g_sched_exit_longjmp = false;  // let os_sched_exit return (cover trailing return 0)

    int ret = handler_sign_tx(&cdata, 1, false, false);

    TEST_ASSERT_TRUE(g_validate_called);
    TEST_ASSERT_TRUE(g_validate_choice);
    TEST_ASSERT_EQUAL(0, ret);
}

void test_swap_double_sign_guard(void) {
    buffer_t cdata = make_cdata();
    G_context.req_type = CONFIRM_TRANSACTION;
    G_called_from_swap = true;
    G_swap_response_ready = true;  // already signed once -> safety exit

    if (setjmp(g_exit_jmp) == 0) {
        handler_sign_tx(&cdata, 1, false, false);
        TEST_FAIL_MESSAGE("expected os_sched_exit on double-sign guard");
    }
    TEST_ASSERT_FALSE(g_validate_called);  // never reached signing
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_chunk0_success);
    RUN_TEST(test_chunk0_bad_path_len);
    RUN_TEST(test_chunk0_bad_path);

    RUN_TEST(test_accumulate_wrong_req_type);
    RUN_TEST(test_accumulate_overflow);
    RUN_TEST(test_accumulate_buffer_move_failure);
    RUN_TEST(test_accumulate_more_chunks);

    RUN_TEST(test_process_deserialize_failure);
    RUN_TEST(test_process_token_not_found);
    RUN_TEST(test_process_hash_failure);

    RUN_TEST(test_display_standard);
    RUN_TEST(test_display_blind_sign);
    RUN_TEST(test_display_token);

    RUN_TEST(test_swap_process_error_sends_swap_error);
    RUN_TEST(test_swap_valid_signs_and_exits);
    RUN_TEST(test_swap_double_sign_guard);

    return UNITY_END();
}
