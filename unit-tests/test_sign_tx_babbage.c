// Unit tests for Babbage era (reference inputs, inline datums)

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <cmocka.h>

#include "apdu/dispatcher.h"
#include "handler/sign_tx.h"
#include "buffer.h"
#include "cardano_swo.h"
#include "cardano_constants.h"
#include "globals.h"
#include "display.h"
#include "transaction/tx.h"
#include "transaction/tx_parse.h"
#include "securityPolicy/securityPolicy.h"
#include "hexUtils.h"
#include "utils/utils.h"
#include "blake2b.h"
#include "cardano_constants.h"
#include "init_apdu.h"

#include "test_sign_tx_fixtures_babbage.h"

// Define ARRAY_LEN macro if not already defined
#ifndef ARRAY_LEN
#define ARRAY_LEN(a) (sizeof(a) / sizeof(a[0]))
#endif

// ----------------------------------------------------------------------
// Simple mocks for IO and UI plumbing
// ----------------------------------------------------------------------

static uint8_t g_last_response[TX_HASH_LENGTH];
static size_t g_last_response_len = 0;
static uint16_t g_last_response_sw = 0;

#include "test_sign_tx_common.h"

int io_send_response_pointer(const uint8_t *buffer, size_t bufferLength, uint16_t sw) {
    assert_true(bufferLength <= sizeof(g_last_response));
    memcpy(g_last_response, buffer, bufferLength);
    g_last_response_len = bufferLength;
    g_last_response_sw = sw;
    return 0;
}

void nbgl_useCaseSpinner(const char *text) {
    (void) text;
}

void nbgl_useCaseStatus(const char *text, bool success, void (*callback)(void)) {
    (void) text;
    (void) success;
    if (callback != NULL) {
        callback();
    }
}

typedef enum {
    STATUS_TYPE_TRANSACTION_SIGNED = 0,
    STATUS_TYPE_TRANSACTION_REJECTED = 1,
} nbgl_reviewStatusType_t;

void nbgl_useCaseReviewStatus(nbgl_reviewStatusType_t reviewStatusType, void (*callback)(void)) {
    (void) reviewStatusType;
    if (callback != NULL) {
        callback();
    }
}

void ui_menu_main(void) {
    // no-op
}

// Display helper that immediately approves the transaction
int ui_display_transaction(void) {
    io_send_response_pointer(G_context.tx_info.tx_hash, sizeof(G_context.tx_info.tx_hash), SWO_SUCCESS);
    G_context.state.tx_state = TX_STATE_APPROVED;
    G_context.req_type = REQUEST_NONE;
    return 0;  // UI functions return 0 on success
}
int ui_display_witness(const bip44_path_t *witnessPath,
                       security_policy_t securityPolicy,
                       warning_bits_t warnings) {
    (void) witnessPath;
    (void) securityPolicy;
    (void) warnings;
    finalize_witness();
    return SWO_SUCCESS;
}

// app_mem_* implementations backed by malloc/free
bool app_mem_init(void) {
    return true;
}

void *app_mem_alloc_impl(size_t size, bool persistent, const char *file, int line) {
    (void) persistent;
    (void) file;
    (void) line;
    return malloc(size);
}

void app_mem_free_impl(void *ptr, const char *file, int line) {
    (void) file;
    (void) line;
    free(ptr);
}

void app_mem_dump_stats(void) {
    // no-op
}

// Device-owned output indicator (from tx.h)
enum {
    OUTPUT_DESTINATION_TYPE_THIRD_PARTY = 1,
    OUTPUT_DESTINATION_TYPE_DEVICE_OWNED = 2,
};

// ======================================================================
// BABBAGE Era Tests
// ======================================================================

static void test_sign_tx_with_short_inline_datum_in_output_with_tokens_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_SHORT_INLINE_DATUM_IN_OUTPUT_WITH_TOKENS, false);
}

static void test_sign_tx_with_short_inline_datum_in_output_with_tokens_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_SHORT_INLINE_DATUM_IN_OUTPUT_WITH_TOKENS, true);
}

static void test_sign_tx_with_long_inline_datum_480_b_in_output_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_LONG_INLINE_DATUM_480_B_IN_OUTPUT, false);
}

static void test_sign_tx_with_long_inline_datum_480_b_in_output_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_LONG_INLINE_DATUM_480_B_IN_OUTPUT, true);
}

static void test_sign_tx_with_long_inline_datum_304_b_in_output_with_tokens_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_LONG_INLINE_DATUM_304_B_IN_OUTPUT_WITH_TOKENS, false);
}

static void test_sign_tx_with_long_inline_datum_304_b_in_output_with_tokens_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_LONG_INLINE_DATUM_304_B_IN_OUTPUT_WITH_TOKENS, true);
}

static void test_sign_tx_with_datum_hash_and_short_ref_script_in_output_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_DATUM_HASH_AND_SHORT_REF_SCRIPT_IN_OUTPUT, false);
}

static void test_sign_tx_with_datum_hash_and_short_ref_script_in_output_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_DATUM_HASH_AND_SHORT_REF_SCRIPT_IN_OUTPUT, true);
}

static void test_sign_tx_with_datum_hash_and_ref_script_240_b_in_output_in_babbage_format_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_DATUM_HASH_AND_REF_SCRIPT_240_B_IN_OUTPUT_IN_BABBAGE_FORMAT, false);
}

static void test_sign_tx_with_datum_hash_and_ref_script_240_b_in_output_in_babbage_format_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_DATUM_HASH_AND_REF_SCRIPT_240_B_IN_OUTPUT_IN_BABBAGE_FORMAT, true);
}

static void test_sign_tx_with_datum_hash_and_script_reference_304_b_in_output_as_map_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_DATUM_HASH_AND_SCRIPT_REFERENCE_304_B_IN_OUTPUT_AS_MAP, false);
}

static void test_sign_tx_with_datum_hash_and_script_reference_304_b_in_output_as_map_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_DATUM_HASH_AND_SCRIPT_REFERENCE_304_B_IN_OUTPUT_AS_MAP, true);
}

static void test_sign_tx_with_datum_hash_in_output_with_tokens_in_babbage_format_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_WITH_TOKENS_IN_BABBAGE_FORMAT, false);
}

static void test_sign_tx_with_datum_hash_in_output_with_tokens_in_babbage_format_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_WITH_TOKENS_IN_BABBAGE_FORMAT, true);
}

static void test_sign_tx_with_a_complex_multiasset_output_babbage_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_A_COMPLEX_MULTIASSET_OUTPUT_BABBAGE, false);
}

static void test_sign_tx_with_a_complex_multiasset_output_babbage_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_A_COMPLEX_MULTIASSET_OUTPUT_BABBAGE, true);
}

static void test_sign_tx_with_change_output_as_map_and_multiple_reference_inputs_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_CHANGE_OUTPUT_AS_MAP_AND_MULTIPLE_REFERENCE_INPUTS, false);
}

static void test_sign_tx_with_change_output_as_map_and_multiple_reference_inputs_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_CHANGE_OUTPUT_AS_MAP_AND_MULTIPLE_REFERENCE_INPUTS, true);
}

static void test_sign_tx_with_change_output_as_map_and_total_collateral_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_CHANGE_OUTPUT_AS_MAP_AND_TOTAL_COLLATERAL, false);
}

static void test_sign_tx_with_change_output_as_map_and_total_collateral_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_CHANGE_OUTPUT_AS_MAP_AND_TOTAL_COLLATERAL, true);
}

static void test_sign_tx_with_change_output_as_map_and_collateral_output_as_array_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_CHANGE_OUTPUT_AS_MAP_AND_COLLATERAL_OUTPUT_AS_ARRAY, false);
}

static void test_sign_tx_with_change_output_as_map_and_collateral_output_as_array_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_CHANGE_OUTPUT_AS_MAP_AND_COLLATERAL_OUTPUT_AS_ARRAY, true);
}

static void test_sign_tx_with_change_collateral_output_as_map_without_total_collateral_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_CHANGE_COLLATERAL_OUTPUT_AS_MAP_WITHOUT_TOTAL_COLLATERAL, false);
}

static void test_sign_tx_with_change_collateral_output_as_map_without_total_collateral_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_CHANGE_COLLATERAL_OUTPUT_AS_MAP_WITHOUT_TOTAL_COLLATERAL, true);
}

static void test_sign_tx_with_change_collateral_output_as_map_with_total_collateral_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_CHANGE_COLLATERAL_OUTPUT_AS_MAP_WITH_TOTAL_COLLATERAL, false);
}

static void test_sign_tx_with_change_collateral_output_as_map_with_total_collateral_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_CHANGE_COLLATERAL_OUTPUT_AS_MAP_WITH_TOTAL_COLLATERAL, true);
}

static void test_sign_tx_with_thirdparty_collateral_output_as_map_without_total_collateral_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_THIRDPARTY_COLLATERAL_OUTPUT_AS_MAP_WITHOUT_TOTAL_COLLATERAL, false);
}

static void test_sign_tx_with_thirdparty_collateral_output_as_map_without_total_collateral_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_THIRDPARTY_COLLATERAL_OUTPUT_AS_MAP_WITHOUT_TOTAL_COLLATERAL, true);
}

static void test_sign_tx_with_thirdparty_collateral_output_as_map_with_total_collateral_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_THIRDPARTY_COLLATERAL_OUTPUT_AS_MAP_WITH_TOTAL_COLLATERAL, false);
}

static void test_sign_tx_with_thirdparty_collateral_output_as_map_with_total_collateral_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_SIGN_TX_WITH_THIRDPARTY_COLLATERAL_OUTPUT_AS_MAP_WITH_TOTAL_COLLATERAL, true);
}

static void test_full_test_for_trezor_feature_parity_babbage_elements_plutus_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_FULL_TEST_FOR_TREZOR_FEATURE_PARITY_BABBAGE_ELEMENTS_PLUTUS, false);
}

static void test_full_test_for_trezor_feature_parity_babbage_elements_plutus_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_FULL_TEST_FOR_TREZOR_FEATURE_PARITY_BABBAGE_ELEMENTS_PLUTUS, true);
}

static void test_full_test_for_trezor_feature_parity_babbage_elements_ordinary_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_FULL_TEST_FOR_TREZOR_FEATURE_PARITY_BABBAGE_ELEMENTS_ORDINARY, false);
}

static void test_full_test_for_trezor_feature_parity_babbage_elements_ordinary_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_BABBAGE_FULL_TEST_FOR_TREZOR_FEATURE_PARITY_BABBAGE_ELEMENTS_ORDINARY, true);
}

// ======================================================================
// Main
// ======================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sign_tx_with_short_inline_datum_in_output_with_tokens_expert_off),
        cmocka_unit_test(test_sign_tx_with_short_inline_datum_in_output_with_tokens_expert_on),
        cmocka_unit_test(test_sign_tx_with_long_inline_datum_480_b_in_output_expert_off),
        cmocka_unit_test(test_sign_tx_with_long_inline_datum_480_b_in_output_expert_on),
        cmocka_unit_test(test_sign_tx_with_long_inline_datum_304_b_in_output_with_tokens_expert_off),
        cmocka_unit_test(test_sign_tx_with_long_inline_datum_304_b_in_output_with_tokens_expert_on),
        cmocka_unit_test(test_sign_tx_with_datum_hash_and_short_ref_script_in_output_expert_off),
        cmocka_unit_test(test_sign_tx_with_datum_hash_and_short_ref_script_in_output_expert_on),
        cmocka_unit_test(test_sign_tx_with_datum_hash_and_ref_script_240_b_in_output_in_babbage_format_expert_off),
        cmocka_unit_test(test_sign_tx_with_datum_hash_and_ref_script_240_b_in_output_in_babbage_format_expert_on),
        cmocka_unit_test(test_sign_tx_with_datum_hash_and_script_reference_304_b_in_output_as_map_expert_off),
        cmocka_unit_test(test_sign_tx_with_datum_hash_and_script_reference_304_b_in_output_as_map_expert_on),
        cmocka_unit_test(test_sign_tx_with_datum_hash_in_output_with_tokens_in_babbage_format_expert_off),
        cmocka_unit_test(test_sign_tx_with_datum_hash_in_output_with_tokens_in_babbage_format_expert_on),
        cmocka_unit_test(test_sign_tx_with_a_complex_multiasset_output_babbage_expert_off),
        cmocka_unit_test(test_sign_tx_with_a_complex_multiasset_output_babbage_expert_on),
        cmocka_unit_test(test_sign_tx_with_change_output_as_map_and_multiple_reference_inputs_expert_off),
        cmocka_unit_test(test_sign_tx_with_change_output_as_map_and_multiple_reference_inputs_expert_on),
        cmocka_unit_test(test_sign_tx_with_change_output_as_map_and_total_collateral_expert_off),
        cmocka_unit_test(test_sign_tx_with_change_output_as_map_and_total_collateral_expert_on),
        cmocka_unit_test(test_sign_tx_with_change_output_as_map_and_collateral_output_as_array_expert_off),
        cmocka_unit_test(test_sign_tx_with_change_output_as_map_and_collateral_output_as_array_expert_on),
        cmocka_unit_test(test_sign_tx_with_change_collateral_output_as_map_without_total_collateral_expert_off),
        cmocka_unit_test(test_sign_tx_with_change_collateral_output_as_map_without_total_collateral_expert_on),
        cmocka_unit_test(test_sign_tx_with_change_collateral_output_as_map_with_total_collateral_expert_off),
        cmocka_unit_test(test_sign_tx_with_change_collateral_output_as_map_with_total_collateral_expert_on),
        cmocka_unit_test(test_sign_tx_with_thirdparty_collateral_output_as_map_without_total_collateral_expert_off),
        cmocka_unit_test(test_sign_tx_with_thirdparty_collateral_output_as_map_without_total_collateral_expert_on),
        cmocka_unit_test(test_sign_tx_with_thirdparty_collateral_output_as_map_with_total_collateral_expert_off),
        cmocka_unit_test(test_sign_tx_with_thirdparty_collateral_output_as_map_with_total_collateral_expert_on),
        cmocka_unit_test(test_full_test_for_trezor_feature_parity_babbage_elements_plutus_expert_off),
        cmocka_unit_test(test_full_test_for_trezor_feature_parity_babbage_elements_plutus_expert_on),
        cmocka_unit_test(test_full_test_for_trezor_feature_parity_babbage_elements_ordinary_expert_off),
        cmocka_unit_test(test_full_test_for_trezor_feature_parity_babbage_elements_ordinary_expert_on),
    };
    return _cmocka_run_group_tests("test_sign_tx_babbage", tests, ARRAY_LEN(tests), NULL, NULL);
}
