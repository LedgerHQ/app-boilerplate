// Unit tests for Byron era transaction signing

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

#include "test_sign_tx_fixtures_byron.h"

// Macro for array length if not already defined
#ifndef ARRAY_LEN
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

// ======================================================================
// Simple mocks for IO and UI plumbing
// ======================================================================

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

int ui_display_transaction(void) {
    io_send_response_pointer(G_context.tx_info.tx_hash, sizeof(G_context.tx_info.tx_hash), SWO_SUCCESS);
    G_context.state.tx_state = TX_STATE_APPROVED;
    G_context.req_type = REQUEST_NONE;
    tx_review_cleanup();
    return 0;
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

// ======================================================================
// Helper functions
// ======================================================================

static void reset_context(void) {
    memset(&G_context, 0, sizeof(G_context));
    g_last_response_len = 0;
    g_last_response_sw = 0;
}

static void build_init_apdu(uint16_t num_inputs,
                           uint16_t num_outputs,
                           uint16_t num_witnesses,
                           bool include_ttl,
                           bool include_validity_interval_start,
                           uint32_t protocol_magic,
                           uint8_t network_id,
                           uint8_t *init_raw,
                           size_t *out_len) {
    const char *init_hex_template =
        "0000000000000000"
        "%02X"
        "%08X"
        "03"
        "%04X"
        "%04X"
        "%02X"
        "0000"
        "0000"
        "01"
        "%02X"
        "0000"
        "01"
        "0000"
        "0000"
        "01"
        "01"
        "01"
        "0000"
        "0000"
        "01"
        "01"
        "%04X";

    char init_hex[512];
    snprintf(init_hex, sizeof(init_hex), init_hex_template,
             network_id,
             protocol_magic,
             num_inputs,
             num_outputs,
             include_ttl ? ITEM_INCLUDED_YES : ITEM_INCLUDED_NO,
             include_validity_interval_start ? ITEM_INCLUDED_YES : ITEM_INCLUDED_NO,
             num_witnesses);

    *out_len = hex_to_bytes(init_hex, init_raw, 512);
}

static void run_byron_test(const uint8_t *raw_tx,
                          size_t raw_tx_len,
                          const char *cbor_hex,
                          const char *expected_hash_hex,
                          uint16_t num_inputs,
                          uint16_t num_outputs,
                          uint16_t num_witnesses,
                          bool include_ttl,
                          bool include_validity_interval_start,
                          uint32_t protocol_magic,
                          uint8_t network_id) {
    reset_context();
    assert_true(app_mem_init());

    uint8_t init_raw[512];
    size_t init_len;
    build_init_apdu(
        num_inputs,
        num_outputs,
        num_witnesses,
        include_ttl,
        include_validity_interval_start,
        protocol_magic,
        network_id,
        init_raw,
        &init_len
    );

    buffer_t init_buf = {
        .ptr = init_raw,
        .size = init_len,
        .offset = 0,
    };

    assert_int_equal(handler_sign_tx(&init_buf, 0x00, false), 0);
    assert_int_equal(G_context.req_type, REQUEST_SIGN_TRANSACTION);
    assert_int_equal(G_context.state.tx_state, TX_STATE_CHUNKS);
    assert_int_equal(G_context.tx_info.num_witnesses, num_witnesses);
    assert_int_equal(G_context.tx_info.transaction.includeTtl, include_ttl);
    assert_int_equal(G_context.tx_info.transaction.includeValidityIntervalStart, include_validity_interval_start);

    buffer_t tx_buf = {
        .ptr = (uint8_t *)raw_tx,
        .size = raw_tx_len,
        .offset = 0,
    };
    assert_int_equal(handler_sign_tx(&tx_buf, 0x02, false), 0);

    uint8_t expected_cbor[2048];
    size_t cbor_len = hex_to_bytes(cbor_hex, expected_cbor, sizeof(expected_cbor));

    uint8_t expected_hash[TX_HASH_LENGTH];
    assert_int_equal(blake2b(expected_hash, TX_HASH_LENGTH, expected_cbor, cbor_len), 0);

    assert_int_equal(g_last_response_len, TX_HASH_LENGTH);
    assert_memory_equal(g_last_response, expected_hash, TX_HASH_LENGTH);
    assert_int_equal(g_last_response_sw, SWO_SUCCESS);
    assert_int_equal(G_context.req_type, REQUEST_NONE);

    uint8_t expected_hash_from_fixture[TX_HASH_LENGTH];
    hex_to_bytes(expected_hash_hex, expected_hash_from_fixture, sizeof(expected_hash_from_fixture));
    assert_memory_equal(expected_hash, expected_hash_from_fixture, TX_HASH_LENGTH);

    tx_context_cleanup();
}

// ======================================================================
// Byron Era Tests
// ======================================================================

static void test_sign_tx_with_third_party_byron_mainnet_output(void **state) {
    (void) state;
    run_byron_test(FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_MAINNET_OUTPUT_RAW_TX,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_MAINNET_OUTPUT_RAW_TX_LEN,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_MAINNET_OUTPUT_TX_BODY_CBOR_HEX,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_MAINNET_OUTPUT_EXPECTED_HASH_HEX,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_MAINNET_OUTPUT_NUM_INPUTS,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_MAINNET_OUTPUT_NUM_OUTPUTS,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_MAINNET_OUTPUT_NUM_WITNESSES,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_MAINNET_OUTPUT_INCLUDE_TTL,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_MAINNET_OUTPUT_INCLUDE_VALIDITY_INTERVAL_START,
                  0x2D964A09,
                  0x01);
}

static void test_sign_tx_with_third_party_byron_daedalus_mainnet_output(void **state) {
    (void) state;
    run_byron_test(FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_DAEDALUS_MAINNET_OUTPUT_RAW_TX,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_DAEDALUS_MAINNET_OUTPUT_RAW_TX_LEN,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_DAEDALUS_MAINNET_OUTPUT_TX_BODY_CBOR_HEX,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_DAEDALUS_MAINNET_OUTPUT_EXPECTED_HASH_HEX,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_DAEDALUS_MAINNET_OUTPUT_NUM_INPUTS,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_DAEDALUS_MAINNET_OUTPUT_NUM_OUTPUTS,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_DAEDALUS_MAINNET_OUTPUT_NUM_WITNESSES,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_DAEDALUS_MAINNET_OUTPUT_INCLUDE_TTL,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_DAEDALUS_MAINNET_OUTPUT_INCLUDE_VALIDITY_INTERVAL_START,
                  0x2D964A09,
                  0x01);
}

static void test_sign_tx_with_third_party_byron_testnet_output(void **state) {
    (void) state;
    run_byron_test(FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_TESTNET_OUTPUT_RAW_TX,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_TESTNET_OUTPUT_RAW_TX_LEN,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_TESTNET_OUTPUT_TX_BODY_CBOR_HEX,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_TESTNET_OUTPUT_EXPECTED_HASH_HEX,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_TESTNET_OUTPUT_NUM_INPUTS,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_TESTNET_OUTPUT_NUM_OUTPUTS,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_TESTNET_OUTPUT_NUM_WITNESSES,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_TESTNET_OUTPUT_INCLUDE_TTL,
                  FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_TESTNET_OUTPUT_INCLUDE_VALIDITY_INTERVAL_START,
                  0x2A,
                  0x00);
}

// ======================================================================
// Main
// ======================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sign_tx_with_third_party_byron_mainnet_output),
        cmocka_unit_test(test_sign_tx_with_third_party_byron_daedalus_mainnet_output),
        cmocka_unit_test(test_sign_tx_with_third_party_byron_testnet_output),
    };
    return _cmocka_run_group_tests("test_sign_tx_byron", tests, ARRAY_LEN(tests), NULL, NULL);
}
