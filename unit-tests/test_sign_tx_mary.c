// Unit tests for Mary era transaction signing (multiasset/tokens)

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

#include "test_sign_tx_fixtures_mary.h"

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

// ----------------------------------------------------------------------
// Helper functions
// ----------------------------------------------------------------------

static void reset_context(void) {
    memset(&G_context, 0, sizeof(G_context));
    g_last_response_len = 0;
    g_last_response_sw = 0;
}

// Helper to build INIT APDU for Mary transactions
static void build_init_apdu(uint16_t num_inputs,
                           uint16_t num_outputs,
                           uint16_t num_witnesses,
                           bool include_ttl,
                           bool include_validity_interval_start,
                           uint16_t num_mint_asset_groups,
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
        "%02X"                          // include_ttl (ITEM_INCLUDED_YES/NO)
        "0000"                          // num_certificates = 0
        "0000"                          // num_withdrawals = 0
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
             include_validity_interval_start ? ITEM_INCLUDED_YES : ITEM_INCLUDED_NO,
             num_mint_asset_groups,
             num_witnesses);

    *out_len = hex_to_bytes(init_hex, init_raw, 512);
}

// Generic test runner for Mary fixtures
static void run_mary_test(const char *fixture_prefix,
                         const uint8_t *raw_tx,
                         size_t raw_tx_len,
                         const char *cbor_hex,
                         const char *expected_hash_hex,
                         uint16_t num_inputs,
                         uint16_t num_outputs,
                         uint16_t num_witnesses,
                         bool include_ttl,
                         bool include_validity_interval_start,
                         uint16_t num_mint_asset_groups) {
    reset_context();
    assert_true(app_mem_init());

    // Build INIT APDU dynamically
    uint8_t init_raw[512];
    size_t init_len;
    build_init_apdu(
        num_inputs,
        num_outputs,
        num_witnesses,
        include_ttl,
        include_validity_interval_start,
        num_mint_asset_groups,
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
// Mary Era Tests
// ======================================================================

static void test_sign_tx_with_a_multiasset_output(void **state) {
    (void) state;
    run_mary_test("FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_OUTPUT",
                 FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_OUTPUT_RAW_TX,
                 FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_OUTPUT_RAW_TX_LEN,
                 FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_OUTPUT_TX_BODY_CBOR_HEX,
                 FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_OUTPUT_EXPECTED_HASH_HEX,
                 FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_OUTPUT_NUM_INPUTS,
                 FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_OUTPUT_NUM_OUTPUTS,
                 FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_OUTPUT_NUM_WITNESSES,
                 FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_OUTPUT_INCLUDE_TTL,
                 FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_OUTPUT_INCLUDE_VALIDITY_INTERVAL_START,
                 FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_OUTPUT_NUM_MINT_ASSET_GROUPS);
}

static void test_sign_tx_with_a_complex_multiasset_output(void **state) {
    (void) state;
    run_mary_test("FIXTURE_MARY_SIGN_TX_WITH_A_COMPLEX_MULTIASSET_OUTPUT",
                 FIXTURE_MARY_SIGN_TX_WITH_A_COMPLEX_MULTIASSET_OUTPUT_RAW_TX,
                 FIXTURE_MARY_SIGN_TX_WITH_A_COMPLEX_MULTIASSET_OUTPUT_RAW_TX_LEN,
                 FIXTURE_MARY_SIGN_TX_WITH_A_COMPLEX_MULTIASSET_OUTPUT_TX_BODY_CBOR_HEX,
                 FIXTURE_MARY_SIGN_TX_WITH_A_COMPLEX_MULTIASSET_OUTPUT_EXPECTED_HASH_HEX,
                 FIXTURE_MARY_SIGN_TX_WITH_A_COMPLEX_MULTIASSET_OUTPUT_NUM_INPUTS,
                 FIXTURE_MARY_SIGN_TX_WITH_A_COMPLEX_MULTIASSET_OUTPUT_NUM_OUTPUTS,
                 FIXTURE_MARY_SIGN_TX_WITH_A_COMPLEX_MULTIASSET_OUTPUT_NUM_WITNESSES,
                 FIXTURE_MARY_SIGN_TX_WITH_A_COMPLEX_MULTIASSET_OUTPUT_INCLUDE_TTL,
                 FIXTURE_MARY_SIGN_TX_WITH_A_COMPLEX_MULTIASSET_OUTPUT_INCLUDE_VALIDITY_INTERVAL_START,
                 FIXTURE_MARY_SIGN_TX_WITH_A_COMPLEX_MULTIASSET_OUTPUT_NUM_MINT_ASSET_GROUPS);
}

static void test_sign_tx_with_big_numbers(void **state) {
    (void) state;
    run_mary_test("FIXTURE_MARY_SIGN_TX_WITH_BIG_NUMBERS",
                 FIXTURE_MARY_SIGN_TX_WITH_BIG_NUMBERS_RAW_TX,
                 FIXTURE_MARY_SIGN_TX_WITH_BIG_NUMBERS_RAW_TX_LEN,
                 FIXTURE_MARY_SIGN_TX_WITH_BIG_NUMBERS_TX_BODY_CBOR_HEX,
                 FIXTURE_MARY_SIGN_TX_WITH_BIG_NUMBERS_EXPECTED_HASH_HEX,
                 FIXTURE_MARY_SIGN_TX_WITH_BIG_NUMBERS_NUM_INPUTS,
                 FIXTURE_MARY_SIGN_TX_WITH_BIG_NUMBERS_NUM_OUTPUTS,
                 FIXTURE_MARY_SIGN_TX_WITH_BIG_NUMBERS_NUM_WITNESSES,
                 FIXTURE_MARY_SIGN_TX_WITH_BIG_NUMBERS_INCLUDE_TTL,
                 FIXTURE_MARY_SIGN_TX_WITH_BIG_NUMBERS_INCLUDE_VALIDITY_INTERVAL_START,
                 FIXTURE_MARY_SIGN_TX_WITH_BIG_NUMBERS_NUM_MINT_ASSET_GROUPS);
}

static void test_sign_tx_with_a_multiasset_change_output(void **state) {
    (void) state;
    run_mary_test("FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_CHANGE_OUTPUT",
                 FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_CHANGE_OUTPUT_RAW_TX,
                 FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_CHANGE_OUTPUT_RAW_TX_LEN,
                 FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_CHANGE_OUTPUT_TX_BODY_CBOR_HEX,
                 FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_CHANGE_OUTPUT_EXPECTED_HASH_HEX,
                 FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_CHANGE_OUTPUT_NUM_INPUTS,
                 FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_CHANGE_OUTPUT_NUM_OUTPUTS,
                 FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_CHANGE_OUTPUT_NUM_WITNESSES,
                 FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_CHANGE_OUTPUT_INCLUDE_TTL,
                 FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_CHANGE_OUTPUT_INCLUDE_VALIDITY_INTERVAL_START,
                 FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_CHANGE_OUTPUT_NUM_MINT_ASSET_GROUPS);
}

static void test_sign_tx_with_zero_fee_ttl_and_validity_interval_start(void **state) {
    (void) state;
    run_mary_test("FIXTURE_MARY_SIGN_TX_WITH_ZERO_FEE_TTL_AND_VALIDITY_INTERVAL_START",
                 FIXTURE_MARY_SIGN_TX_WITH_ZERO_FEE_TTL_AND_VALIDITY_INTERVAL_START_RAW_TX,
                 FIXTURE_MARY_SIGN_TX_WITH_ZERO_FEE_TTL_AND_VALIDITY_INTERVAL_START_RAW_TX_LEN,
                 FIXTURE_MARY_SIGN_TX_WITH_ZERO_FEE_TTL_AND_VALIDITY_INTERVAL_START_TX_BODY_CBOR_HEX,
                 FIXTURE_MARY_SIGN_TX_WITH_ZERO_FEE_TTL_AND_VALIDITY_INTERVAL_START_EXPECTED_HASH_HEX,
                 FIXTURE_MARY_SIGN_TX_WITH_ZERO_FEE_TTL_AND_VALIDITY_INTERVAL_START_NUM_INPUTS,
                 FIXTURE_MARY_SIGN_TX_WITH_ZERO_FEE_TTL_AND_VALIDITY_INTERVAL_START_NUM_OUTPUTS,
                 FIXTURE_MARY_SIGN_TX_WITH_ZERO_FEE_TTL_AND_VALIDITY_INTERVAL_START_NUM_WITNESSES,
                 FIXTURE_MARY_SIGN_TX_WITH_ZERO_FEE_TTL_AND_VALIDITY_INTERVAL_START_INCLUDE_TTL,
                 FIXTURE_MARY_SIGN_TX_WITH_ZERO_FEE_TTL_AND_VALIDITY_INTERVAL_START_INCLUDE_VALIDITY_INTERVAL_START,
                 FIXTURE_MARY_SIGN_TX_WITH_ZERO_FEE_TTL_AND_VALIDITY_INTERVAL_START_NUM_MINT_ASSET_GROUPS);
}

static void test_sign_tx_with_output_with_decimal_places(void **state) {
    (void) state;
    run_mary_test("FIXTURE_MARY_SIGN_TX_WITH_OUTPUT_WITH_DECIMAL_PLACES",
                 FIXTURE_MARY_SIGN_TX_WITH_OUTPUT_WITH_DECIMAL_PLACES_RAW_TX,
                 FIXTURE_MARY_SIGN_TX_WITH_OUTPUT_WITH_DECIMAL_PLACES_RAW_TX_LEN,
                 FIXTURE_MARY_SIGN_TX_WITH_OUTPUT_WITH_DECIMAL_PLACES_TX_BODY_CBOR_HEX,
                 FIXTURE_MARY_SIGN_TX_WITH_OUTPUT_WITH_DECIMAL_PLACES_EXPECTED_HASH_HEX,
                 FIXTURE_MARY_SIGN_TX_WITH_OUTPUT_WITH_DECIMAL_PLACES_NUM_INPUTS,
                 FIXTURE_MARY_SIGN_TX_WITH_OUTPUT_WITH_DECIMAL_PLACES_NUM_OUTPUTS,
                 FIXTURE_MARY_SIGN_TX_WITH_OUTPUT_WITH_DECIMAL_PLACES_NUM_WITNESSES,
                 FIXTURE_MARY_SIGN_TX_WITH_OUTPUT_WITH_DECIMAL_PLACES_INCLUDE_TTL,
                 FIXTURE_MARY_SIGN_TX_WITH_OUTPUT_WITH_DECIMAL_PLACES_INCLUDE_VALIDITY_INTERVAL_START,
                 FIXTURE_MARY_SIGN_TX_WITH_OUTPUT_WITH_DECIMAL_PLACES_NUM_MINT_ASSET_GROUPS);
}

static void test_sign_tx_with_mint_fields_with_various_amounts(void **state) {
    (void) state;
    run_mary_test("FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_WITH_VARIOUS_AMOUNTS",
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_WITH_VARIOUS_AMOUNTS_RAW_TX,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_WITH_VARIOUS_AMOUNTS_RAW_TX_LEN,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_WITH_VARIOUS_AMOUNTS_TX_BODY_CBOR_HEX,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_WITH_VARIOUS_AMOUNTS_EXPECTED_HASH_HEX,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_WITH_VARIOUS_AMOUNTS_NUM_INPUTS,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_WITH_VARIOUS_AMOUNTS_NUM_OUTPUTS,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_WITH_VARIOUS_AMOUNTS_NUM_WITNESSES,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_WITH_VARIOUS_AMOUNTS_INCLUDE_TTL,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_WITH_VARIOUS_AMOUNTS_INCLUDE_VALIDITY_INTERVAL_START,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_WITH_VARIOUS_AMOUNTS_NUM_MINT_ASSET_GROUPS);
}

static void test_sign_tx_with_mint_with_decimal_places(void **state) {
    (void) state;
    run_mary_test("FIXTURE_MARY_SIGN_TX_WITH_MINT_WITH_DECIMAL_PLACES",
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_WITH_DECIMAL_PLACES_RAW_TX,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_WITH_DECIMAL_PLACES_RAW_TX_LEN,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_WITH_DECIMAL_PLACES_TX_BODY_CBOR_HEX,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_WITH_DECIMAL_PLACES_EXPECTED_HASH_HEX,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_WITH_DECIMAL_PLACES_NUM_INPUTS,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_WITH_DECIMAL_PLACES_NUM_OUTPUTS,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_WITH_DECIMAL_PLACES_NUM_WITNESSES,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_WITH_DECIMAL_PLACES_INCLUDE_TTL,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_WITH_DECIMAL_PLACES_INCLUDE_VALIDITY_INTERVAL_START,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_WITH_DECIMAL_PLACES_NUM_MINT_ASSET_GROUPS);
}

static void test_sign_tx_with_mint_fields_among_other_fields(void **state) {
    (void) state;
    run_mary_test("FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_AMONG_OTHER_FIELDS",
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_AMONG_OTHER_FIELDS_RAW_TX,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_AMONG_OTHER_FIELDS_RAW_TX_LEN,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_AMONG_OTHER_FIELDS_TX_BODY_CBOR_HEX,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_AMONG_OTHER_FIELDS_EXPECTED_HASH_HEX,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_AMONG_OTHER_FIELDS_NUM_INPUTS,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_AMONG_OTHER_FIELDS_NUM_OUTPUTS,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_AMONG_OTHER_FIELDS_NUM_WITNESSES,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_AMONG_OTHER_FIELDS_INCLUDE_TTL,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_AMONG_OTHER_FIELDS_INCLUDE_VALIDITY_INTERVAL_START,
                 FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_AMONG_OTHER_FIELDS_NUM_MINT_ASSET_GROUPS);
}

// ======================================================================
// Main
// ======================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sign_tx_with_a_multiasset_output),
        cmocka_unit_test(test_sign_tx_with_a_complex_multiasset_output),
        cmocka_unit_test(test_sign_tx_with_big_numbers),
        cmocka_unit_test(test_sign_tx_with_a_multiasset_change_output),
        cmocka_unit_test(test_sign_tx_with_zero_fee_ttl_and_validity_interval_start),
        cmocka_unit_test(test_sign_tx_with_output_with_decimal_places),
        cmocka_unit_test(test_sign_tx_with_mint_fields_with_various_amounts),
        cmocka_unit_test(test_sign_tx_with_mint_with_decimal_places),
        cmocka_unit_test(test_sign_tx_with_mint_fields_among_other_fields),
    };
    return _cmocka_run_group_tests("test_sign_tx_mary", tests, ARRAY_LEN(tests), NULL, NULL);
}
