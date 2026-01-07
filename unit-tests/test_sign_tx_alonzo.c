// Unit tests for Alonzo era (Plutus scripts)

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

#include "test_sign_tx_fixtures_alonzo.h"

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
    return calloc(1, size);
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
// ALONZO Era Tests
// ======================================================================

static void test_sign_tx_with_script_data_hash_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_SCRIPT_DATA_HASH, false);
}

static void test_sign_tx_with_script_data_hash_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_SCRIPT_DATA_HASH, true);
}

static void test_sign_tx_with_change_output_as_array_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_CHANGE_OUTPUT_AS_ARRAY, false);
}

static void test_sign_tx_with_change_output_as_array_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_CHANGE_OUTPUT_AS_ARRAY, true);
}

static void test_sign_tx_with_datum_hash_in_output_as_array_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY, false);
}

static void test_sign_tx_with_datum_hash_in_output_as_array_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY, true);
}

static void test_sign_tx_with_datum_hash_in_output_as_array_with_tokens_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_WITH_TOKENS, false);
}

static void test_sign_tx_with_datum_hash_in_output_as_array_with_tokens_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_WITH_TOKENS, true);
}

static void test_sign_tx_with_missing_datum_hash_in_output_with_tokens_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_MISSING_DATUM_HASH_IN_OUTPUT_WITH_TOKENS, false);
}

static void test_sign_tx_with_missing_datum_hash_in_output_with_tokens_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_MISSING_DATUM_HASH_IN_OUTPUT_WITH_TOKENS, true);
}

static void test_sign_tx_with_collateral_inputs_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_COLLATERAL_INPUTS, false);
}

static void test_sign_tx_with_collateral_inputs_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_COLLATERAL_INPUTS, true);
}

static void test_sign_tx_with_collateral_inputs_shelley_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_COLLATERAL_INPUTS_SHELLEY, false);
}

static void test_sign_tx_with_collateral_inputs_shelley_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_COLLATERAL_INPUTS_SHELLEY, true);
}

static void test_sign_tx_with_required_signers_mixed_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_REQUIRED_SIGNERS_MIXED, false);
}

static void test_sign_tx_with_required_signers_mixed_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_REQUIRED_SIGNERS_MIXED, true);
}

static void test_sign_tx_with_mint_path_in_a_required_signer_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_MINT_PATH_IN_A_REQUIRED_SIGNER, false);
}

static void test_sign_tx_with_mint_path_in_a_required_signer_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_MINT_PATH_IN_A_REQUIRED_SIGNER, true);
}

static void test_sign_tx_with_key_hash_in_stake_credential_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_KEY_HASH_IN_STAKE_CREDENTIAL, false);
}

static void test_sign_tx_with_key_hash_in_stake_credential_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_KEY_HASH_IN_STAKE_CREDENTIAL, true);
}

static void test_full_test_for_trezor_feature_parity_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_FULL_TEST_FOR_TREZOR_FEATURE_PARITY, false);
}

static void test_full_test_for_trezor_feature_parity_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_FULL_TEST_FOR_TREZOR_FEATURE_PARITY, true);
}

static void test_sign_tx_with_multidelegation_keys_in_all_tx_elements_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_MULTIDELEGATION_KEYS_IN_ALL_TX_ELEMENTS, false);
}

static void test_sign_tx_with_multidelegation_keys_in_all_tx_elements_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_SIGN_TX_WITH_MULTIDELEGATION_KEYS_IN_ALL_TX_ELEMENTS, true);
}

// ======================================================================
// Main
// ======================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sign_tx_with_script_data_hash_expert_off),
        cmocka_unit_test(test_sign_tx_with_script_data_hash_expert_on),
        cmocka_unit_test(test_sign_tx_with_change_output_as_array_expert_off),
        cmocka_unit_test(test_sign_tx_with_change_output_as_array_expert_on),
        cmocka_unit_test(test_sign_tx_with_datum_hash_in_output_as_array_expert_off),
        cmocka_unit_test(test_sign_tx_with_datum_hash_in_output_as_array_expert_on),
        cmocka_unit_test(test_sign_tx_with_datum_hash_in_output_as_array_with_tokens_expert_off),
        cmocka_unit_test(test_sign_tx_with_datum_hash_in_output_as_array_with_tokens_expert_on),
        cmocka_unit_test(test_sign_tx_with_missing_datum_hash_in_output_with_tokens_expert_off),
        cmocka_unit_test(test_sign_tx_with_missing_datum_hash_in_output_with_tokens_expert_on),
        cmocka_unit_test(test_sign_tx_with_collateral_inputs_expert_off),
        cmocka_unit_test(test_sign_tx_with_collateral_inputs_expert_on),
        cmocka_unit_test(test_sign_tx_with_collateral_inputs_shelley_expert_off),
        cmocka_unit_test(test_sign_tx_with_collateral_inputs_shelley_expert_on),
        cmocka_unit_test(test_sign_tx_with_required_signers_mixed_expert_off),
        cmocka_unit_test(test_sign_tx_with_required_signers_mixed_expert_on),
        cmocka_unit_test(test_sign_tx_with_mint_path_in_a_required_signer_expert_off),
        cmocka_unit_test(test_sign_tx_with_mint_path_in_a_required_signer_expert_on),
        cmocka_unit_test(test_sign_tx_with_key_hash_in_stake_credential_expert_off),
        cmocka_unit_test(test_sign_tx_with_key_hash_in_stake_credential_expert_on),
        cmocka_unit_test(test_full_test_for_trezor_feature_parity_expert_off),
        cmocka_unit_test(test_full_test_for_trezor_feature_parity_expert_on),
        cmocka_unit_test(test_sign_tx_with_multidelegation_keys_in_all_tx_elements_expert_off),
        cmocka_unit_test(test_sign_tx_with_multidelegation_keys_in_all_tx_elements_expert_on),
    };
    return _cmocka_run_group_tests("test_sign_tx_alonzo", tests, ARRAY_LEN(tests), NULL, NULL);
}
