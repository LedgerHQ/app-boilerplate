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
#include "globals.h"
#include "display.h"
#include "transaction/tx.h"
#include "transaction/tx_parse.h"
#include "securityPolicy/securityPolicy.h"
#include "hexUtils.h"
#include "utils/utils.h"
#include "blake2b.h"

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

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

static void reset_context(void) {
    memset(&G_context, 0, sizeof(G_context));
    g_last_response_len = 0;
    g_last_response_sw = 0;
}

// Helper to build INIT APDU for transactions
static void build_init_apdu(uint16_t num_inputs,
                           uint16_t num_outputs,
                           uint16_t num_witnesses,
                           uint16_t num_certificates,
                           bool include_ttl,
                           bool include_validity_interval_start,
                           uint16_t num_mint_asset_groups,
                           uint16_t num_withdrawals,
                           uint8_t *init_raw,
                           size_t *out_len) {
    // INIT APDU format (from sign_tx.h:234)
    // Note: ITEM_INCLUDED_NO = 1, ITEM_INCLUDED_YES = 2
    const char *init_hex_template =
        "0000000000000000"              // options (8B)
        "01"                            // networkId = 1 (mainnet)
        "2D964A09"                      // protocolMagic = 764824073
        "03"                            // signingMode = ORDINARY_TRANSACTION
        "%04X"                          // num_inputs (u16)
        "%04X"                          // num_outputs (u16)
        "%02X"                          // include_ttl
        "%04X"                          // num_certificates (u16)
        "%04X"                          // num_withdrawals (u16)
        "01"                            // include_aux_data = ITEM_INCLUDED_NO
        "%02X"                          // include_validity_interval_start
        "%04X"                          // num_mint_asset_groups (u16)
        "01"                            // include_script_data_hash = ITEM_INCLUDED_NO
        "0000"                          // num_collateral_inputs = 0
        "0000"                          // num_required_signers = 0
        "01"                            // include_network_id = ITEM_INCLUDED_NO
        "01"                            // include_collateral_output = ITEM_INCLUDED_NO
        "01"                            // include_total_collateral = ITEM_INCLUDED_NO
        "0000"                          // num_reference_inputs = 0
        "0000"                          // num_voting_procedures = 0
        "01"                            // include_treasury = ITEM_INCLUDED_NO
        "01"                            // include_donation = ITEM_INCLUDED_NO
        "%04X";                         // num_witnesses (u16)

    char init_hex[512];
    snprintf(init_hex, sizeof(init_hex), init_hex_template,
             num_inputs,
             num_outputs,
             include_ttl ? ITEM_INCLUDED_YES : ITEM_INCLUDED_NO,
             num_certificates,
             num_withdrawals,
             include_validity_interval_start ? ITEM_INCLUDED_YES : ITEM_INCLUDED_NO,
             num_mint_asset_groups,
             num_witnesses);

    *out_len = hex_to_bytes(init_hex, init_raw, 512);
}

// Generic test runner for era transactions
static void __attribute__((unused)) run_era_test(const char *fixture_prefix,
                        const uint8_t *raw_tx,
                        size_t raw_tx_len,
                        const char *cbor_hex,
                        const char *expected_hash_hex,
                        uint16_t num_inputs,
                        uint16_t num_outputs,
                        uint16_t num_witnesses,
                        uint16_t num_certificates,
                        bool include_ttl,
                        bool include_validity_interval_start,
                        uint16_t num_mint_asset_groups,
                        uint16_t num_withdrawals) {
    reset_context();
    assert_true(app_mem_init());

    // Build INIT APDU dynamically
    uint8_t init_raw[512];
    size_t init_len;
    build_init_apdu(
        num_inputs,
        num_outputs,
        num_witnesses,
        num_certificates,
        include_ttl,
        include_validity_interval_start,
        num_mint_asset_groups,
        num_withdrawals,
        init_raw,
        &init_len
    );

    buffer_t init_buf = {
        .ptr = init_raw,
        .size = init_len,
        .offset = 0,
    };

    // Step 1: Send INIT APDU
    assert_int_equal(handler_sign_tx(&init_buf, 0x00, false), 0);  // P1_TX_INIT = 0x00
    assert_int_equal(G_context.req_type, REQUEST_SIGN_TRANSACTION);
    assert_int_equal(G_context.state.tx_state, TX_STATE_CHUNKS);
    assert_int_equal(G_context.tx_info.num_witnesses, num_witnesses);

    // Step 2: Send raw_tx buffer as a single chunk (P1_TX_CHUNK_LAST = 0x02)
    buffer_t tx_buf = {
        .ptr = (uint8_t *)raw_tx,
        .size = raw_tx_len,
        .offset = 0,
    };
    assert_int_equal(handler_sign_tx(&tx_buf, 0x02, false), 0);

    // Step 3: Verify transaction hash
    uint8_t expected_cbor[2048];
    size_t cbor_len = hex_to_bytes(cbor_hex, expected_cbor, sizeof(expected_cbor));

    uint8_t expected_hash[TX_HASH_LENGTH];
    assert_int_equal(blake2b(expected_hash, TX_HASH_LENGTH, expected_cbor, cbor_len), 0);

    assert_int_equal(g_last_response_len, TX_HASH_LENGTH);
    assert_memory_equal(g_last_response, expected_hash, TX_HASH_LENGTH);
    assert_int_equal(g_last_response_sw, SWO_SUCCESS);
    assert_int_equal(G_context.req_type, REQUEST_NONE);

    // Verify hash matches expected value from fixture
    uint8_t expected_hash_from_fixture[TX_HASH_LENGTH];
    hex_to_bytes(expected_hash_hex, expected_hash_from_fixture, sizeof(expected_hash_from_fixture));
    assert_memory_equal(expected_hash, expected_hash_from_fixture, TX_HASH_LENGTH);

    tx_context_cleanup();
}


// ======================================================================
// ALONZO Era Tests
// ======================================================================

static void test_sign_tx_with_script_data_hash(void **state) {
    (void) state;
    run_era_test("FIXTURE_ALONZO_SIGN_TX_WITH_SCRIPT_DATA_HASH",
                 FIXTURE_ALONZO_SIGN_TX_WITH_SCRIPT_DATA_HASH_RAW_TX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_SCRIPT_DATA_HASH_RAW_TX_LEN,
                 FIXTURE_ALONZO_SIGN_TX_WITH_SCRIPT_DATA_HASH_TX_BODY_CBOR_HEX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_SCRIPT_DATA_HASH_EXPECTED_HASH_HEX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_SCRIPT_DATA_HASH_NUM_INPUTS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_SCRIPT_DATA_HASH_NUM_OUTPUTS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_SCRIPT_DATA_HASH_NUM_WITNESSES,
                 FIXTURE_ALONZO_SIGN_TX_WITH_SCRIPT_DATA_HASH_NUM_CERTIFICATES,
                 FIXTURE_ALONZO_SIGN_TX_WITH_SCRIPT_DATA_HASH_INCLUDE_TTL,
                 FIXTURE_ALONZO_SIGN_TX_WITH_SCRIPT_DATA_HASH_INCLUDE_VALIDITY_INTERVAL_START,
                 FIXTURE_ALONZO_SIGN_TX_WITH_SCRIPT_DATA_HASH_NUM_MINT_ASSET_GROUPS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_SCRIPT_DATA_HASH_NUM_WITHDRAWALS);
}

static void test_sign_tx_with_change_output_as_array(void **state) {
    (void) state;
    run_era_test("FIXTURE_ALONZO_SIGN_TX_WITH_CHANGE_OUTPUT_AS_ARRAY",
                 FIXTURE_ALONZO_SIGN_TX_WITH_CHANGE_OUTPUT_AS_ARRAY_RAW_TX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_CHANGE_OUTPUT_AS_ARRAY_RAW_TX_LEN,
                 FIXTURE_ALONZO_SIGN_TX_WITH_CHANGE_OUTPUT_AS_ARRAY_TX_BODY_CBOR_HEX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_CHANGE_OUTPUT_AS_ARRAY_EXPECTED_HASH_HEX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_CHANGE_OUTPUT_AS_ARRAY_NUM_INPUTS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_CHANGE_OUTPUT_AS_ARRAY_NUM_OUTPUTS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_CHANGE_OUTPUT_AS_ARRAY_NUM_WITNESSES,
                 FIXTURE_ALONZO_SIGN_TX_WITH_CHANGE_OUTPUT_AS_ARRAY_NUM_CERTIFICATES,
                 FIXTURE_ALONZO_SIGN_TX_WITH_CHANGE_OUTPUT_AS_ARRAY_INCLUDE_TTL,
                 FIXTURE_ALONZO_SIGN_TX_WITH_CHANGE_OUTPUT_AS_ARRAY_INCLUDE_VALIDITY_INTERVAL_START,
                 FIXTURE_ALONZO_SIGN_TX_WITH_CHANGE_OUTPUT_AS_ARRAY_NUM_MINT_ASSET_GROUPS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_CHANGE_OUTPUT_AS_ARRAY_NUM_WITHDRAWALS);
}

static void test_sign_tx_with_datum_hash_in_output_as_array(void **state) {
    (void) state;
    run_era_test("FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY",
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_RAW_TX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_RAW_TX_LEN,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_TX_BODY_CBOR_HEX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_EXPECTED_HASH_HEX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_NUM_INPUTS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_NUM_OUTPUTS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_NUM_WITNESSES,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_NUM_CERTIFICATES,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_INCLUDE_TTL,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_INCLUDE_VALIDITY_INTERVAL_START,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_NUM_MINT_ASSET_GROUPS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_NUM_WITHDRAWALS);
}

static void test_sign_tx_with_datum_hash_in_output_as_array_with_tokens(void **state) {
    (void) state;
    run_era_test("FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_WITH_TOKENS",
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_WITH_TOKENS_RAW_TX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_WITH_TOKENS_RAW_TX_LEN,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_WITH_TOKENS_TX_BODY_CBOR_HEX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_WITH_TOKENS_EXPECTED_HASH_HEX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_WITH_TOKENS_NUM_INPUTS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_WITH_TOKENS_NUM_OUTPUTS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_WITH_TOKENS_NUM_WITNESSES,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_WITH_TOKENS_NUM_CERTIFICATES,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_WITH_TOKENS_INCLUDE_TTL,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_WITH_TOKENS_INCLUDE_VALIDITY_INTERVAL_START,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_WITH_TOKENS_NUM_MINT_ASSET_GROUPS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_DATUM_HASH_IN_OUTPUT_AS_ARRAY_WITH_TOKENS_NUM_WITHDRAWALS);
}

static void test_sign_tx_with_missing_datum_hash_in_output_with_tokens(void **state) {
    (void) state;
    run_era_test("FIXTURE_ALONZO_SIGN_TX_WITH_MISSING_DATUM_HASH_IN_OUTPUT_WITH_TOKENS",
                 FIXTURE_ALONZO_SIGN_TX_WITH_MISSING_DATUM_HASH_IN_OUTPUT_WITH_TOKENS_RAW_TX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MISSING_DATUM_HASH_IN_OUTPUT_WITH_TOKENS_RAW_TX_LEN,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MISSING_DATUM_HASH_IN_OUTPUT_WITH_TOKENS_TX_BODY_CBOR_HEX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MISSING_DATUM_HASH_IN_OUTPUT_WITH_TOKENS_EXPECTED_HASH_HEX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MISSING_DATUM_HASH_IN_OUTPUT_WITH_TOKENS_NUM_INPUTS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MISSING_DATUM_HASH_IN_OUTPUT_WITH_TOKENS_NUM_OUTPUTS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MISSING_DATUM_HASH_IN_OUTPUT_WITH_TOKENS_NUM_WITNESSES,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MISSING_DATUM_HASH_IN_OUTPUT_WITH_TOKENS_NUM_CERTIFICATES,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MISSING_DATUM_HASH_IN_OUTPUT_WITH_TOKENS_INCLUDE_TTL,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MISSING_DATUM_HASH_IN_OUTPUT_WITH_TOKENS_INCLUDE_VALIDITY_INTERVAL_START,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MISSING_DATUM_HASH_IN_OUTPUT_WITH_TOKENS_NUM_MINT_ASSET_GROUPS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MISSING_DATUM_HASH_IN_OUTPUT_WITH_TOKENS_NUM_WITHDRAWALS);
}

static void test_sign_tx_with_collateral_inputs(void **state) {
    (void) state;
    run_era_test("FIXTURE_ALONZO_SIGN_TX_WITH_COLLATERAL_INPUTS",
                 FIXTURE_ALONZO_SIGN_TX_WITH_COLLATERAL_INPUTS_RAW_TX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_COLLATERAL_INPUTS_RAW_TX_LEN,
                 FIXTURE_ALONZO_SIGN_TX_WITH_COLLATERAL_INPUTS_TX_BODY_CBOR_HEX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_COLLATERAL_INPUTS_EXPECTED_HASH_HEX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_COLLATERAL_INPUTS_NUM_INPUTS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_COLLATERAL_INPUTS_NUM_OUTPUTS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_COLLATERAL_INPUTS_NUM_WITNESSES,
                 FIXTURE_ALONZO_SIGN_TX_WITH_COLLATERAL_INPUTS_NUM_CERTIFICATES,
                 FIXTURE_ALONZO_SIGN_TX_WITH_COLLATERAL_INPUTS_INCLUDE_TTL,
                 FIXTURE_ALONZO_SIGN_TX_WITH_COLLATERAL_INPUTS_INCLUDE_VALIDITY_INTERVAL_START,
                 FIXTURE_ALONZO_SIGN_TX_WITH_COLLATERAL_INPUTS_NUM_MINT_ASSET_GROUPS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_COLLATERAL_INPUTS_NUM_WITHDRAWALS);
}

static void test_sign_tx_with_required_signers_mixed(void **state) {
    (void) state;
    run_era_test("FIXTURE_ALONZO_SIGN_TX_WITH_REQUIRED_SIGNERS_MIXED",
                 FIXTURE_ALONZO_SIGN_TX_WITH_REQUIRED_SIGNERS_MIXED_RAW_TX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_REQUIRED_SIGNERS_MIXED_RAW_TX_LEN,
                 FIXTURE_ALONZO_SIGN_TX_WITH_REQUIRED_SIGNERS_MIXED_TX_BODY_CBOR_HEX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_REQUIRED_SIGNERS_MIXED_EXPECTED_HASH_HEX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_REQUIRED_SIGNERS_MIXED_NUM_INPUTS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_REQUIRED_SIGNERS_MIXED_NUM_OUTPUTS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_REQUIRED_SIGNERS_MIXED_NUM_WITNESSES,
                 FIXTURE_ALONZO_SIGN_TX_WITH_REQUIRED_SIGNERS_MIXED_NUM_CERTIFICATES,
                 FIXTURE_ALONZO_SIGN_TX_WITH_REQUIRED_SIGNERS_MIXED_INCLUDE_TTL,
                 FIXTURE_ALONZO_SIGN_TX_WITH_REQUIRED_SIGNERS_MIXED_INCLUDE_VALIDITY_INTERVAL_START,
                 FIXTURE_ALONZO_SIGN_TX_WITH_REQUIRED_SIGNERS_MIXED_NUM_MINT_ASSET_GROUPS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_REQUIRED_SIGNERS_MIXED_NUM_WITHDRAWALS);
}

static void test_sign_tx_with_mint_path_in_a_required_signer(void **state) {
    (void) state;
    run_era_test("FIXTURE_ALONZO_SIGN_TX_WITH_MINT_PATH_IN_A_REQUIRED_SIGNER",
                 FIXTURE_ALONZO_SIGN_TX_WITH_MINT_PATH_IN_A_REQUIRED_SIGNER_RAW_TX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MINT_PATH_IN_A_REQUIRED_SIGNER_RAW_TX_LEN,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MINT_PATH_IN_A_REQUIRED_SIGNER_TX_BODY_CBOR_HEX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MINT_PATH_IN_A_REQUIRED_SIGNER_EXPECTED_HASH_HEX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MINT_PATH_IN_A_REQUIRED_SIGNER_NUM_INPUTS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MINT_PATH_IN_A_REQUIRED_SIGNER_NUM_OUTPUTS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MINT_PATH_IN_A_REQUIRED_SIGNER_NUM_WITNESSES,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MINT_PATH_IN_A_REQUIRED_SIGNER_NUM_CERTIFICATES,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MINT_PATH_IN_A_REQUIRED_SIGNER_INCLUDE_TTL,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MINT_PATH_IN_A_REQUIRED_SIGNER_INCLUDE_VALIDITY_INTERVAL_START,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MINT_PATH_IN_A_REQUIRED_SIGNER_NUM_MINT_ASSET_GROUPS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_MINT_PATH_IN_A_REQUIRED_SIGNER_NUM_WITHDRAWALS);
}

static void test_sign_tx_with_key_hash_in_stake_credential(void **state) {
    (void) state;
    run_era_test("FIXTURE_ALONZO_SIGN_TX_WITH_KEY_HASH_IN_STAKE_CREDENTIAL",
                 FIXTURE_ALONZO_SIGN_TX_WITH_KEY_HASH_IN_STAKE_CREDENTIAL_RAW_TX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_KEY_HASH_IN_STAKE_CREDENTIAL_RAW_TX_LEN,
                 FIXTURE_ALONZO_SIGN_TX_WITH_KEY_HASH_IN_STAKE_CREDENTIAL_TX_BODY_CBOR_HEX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_KEY_HASH_IN_STAKE_CREDENTIAL_EXPECTED_HASH_HEX,
                 FIXTURE_ALONZO_SIGN_TX_WITH_KEY_HASH_IN_STAKE_CREDENTIAL_NUM_INPUTS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_KEY_HASH_IN_STAKE_CREDENTIAL_NUM_OUTPUTS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_KEY_HASH_IN_STAKE_CREDENTIAL_NUM_WITNESSES,
                 FIXTURE_ALONZO_SIGN_TX_WITH_KEY_HASH_IN_STAKE_CREDENTIAL_NUM_CERTIFICATES,
                 FIXTURE_ALONZO_SIGN_TX_WITH_KEY_HASH_IN_STAKE_CREDENTIAL_INCLUDE_TTL,
                 FIXTURE_ALONZO_SIGN_TX_WITH_KEY_HASH_IN_STAKE_CREDENTIAL_INCLUDE_VALIDITY_INTERVAL_START,
                 FIXTURE_ALONZO_SIGN_TX_WITH_KEY_HASH_IN_STAKE_CREDENTIAL_NUM_MINT_ASSET_GROUPS,
                 FIXTURE_ALONZO_SIGN_TX_WITH_KEY_HASH_IN_STAKE_CREDENTIAL_NUM_WITHDRAWALS);
}

// ======================================================================
// Main
// ======================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sign_tx_with_script_data_hash),
        cmocka_unit_test(test_sign_tx_with_change_output_as_array),
        cmocka_unit_test(test_sign_tx_with_datum_hash_in_output_as_array),
        cmocka_unit_test(test_sign_tx_with_datum_hash_in_output_as_array_with_tokens),
        cmocka_unit_test(test_sign_tx_with_missing_datum_hash_in_output_with_tokens),
        cmocka_unit_test(test_sign_tx_with_collateral_inputs),
        cmocka_unit_test(test_sign_tx_with_required_signers_mixed),
        cmocka_unit_test(test_sign_tx_with_mint_path_in_a_required_signer),
        cmocka_unit_test(test_sign_tx_with_key_hash_in_stake_credential),
    };
    return _cmocka_run_group_tests("test_sign_tx_alonzo", tests, ARRAY_LEN(tests), NULL, NULL);
}
