// Unit tests for Shelley era transaction signing

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

#include "test_sign_tx_fixtures_shelley.h"

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

static inline void append_u8(uint8_t *buffer, size_t *pos, uint8_t value) {
    buffer[(*pos)++] = value;
}

static inline void append_u16_be(uint8_t *buffer, size_t *pos, uint16_t value) {
    buffer[(*pos)++] = (value >> 8) & 0xFF;
    buffer[(*pos)++] = value & 0xFF;
}

static inline void append_u32_be(uint8_t *buffer, size_t *pos, uint32_t value) {
    buffer[(*pos)++] = (value >> 24) & 0xFF;
    buffer[(*pos)++] = (value >> 16) & 0xFF;
    buffer[(*pos)++] = (value >> 8) & 0xFF;
    buffer[(*pos)++] = value & 0xFF;
}

static inline void append_u64_be(uint8_t *buffer, size_t *pos, uint64_t value) {
    for (int i = 7; i >= 0; i--) {
        buffer[(*pos)++] = (value >> (i * 8)) & 0xFF;
    }
}

static void build_init_apdu(uint64_t options,
                           uint16_t num_inputs,
                           uint16_t num_outputs,
                           uint16_t num_witnesses,
                           bool include_ttl,
                           bool include_validity_interval_start,
                           uint16_t num_certificates,
                           uint16_t num_withdrawals,
                           uint16_t num_mint_asset_groups,
                           bool include_aux_data_hash,
                           const uint8_t *aux_data_hash,
                           size_t aux_data_hash_len,
                           uint8_t *init_raw,
                           size_t *out_len) {
    size_t pos = 0;
    append_u64_be(init_raw, &pos, options);
    append_u8(init_raw, &pos, 0x01);          // mainnet network id
    append_u32_be(init_raw, &pos, 0x2D964A09); // mainnet protocol magic
    append_u8(init_raw, &pos, 0x03);          // ordinary signing mode

    append_u16_be(init_raw, &pos, num_inputs);
    append_u16_be(init_raw, &pos, num_outputs);

    append_u8(init_raw, &pos, include_ttl ? ITEM_INCLUDED_YES : ITEM_INCLUDED_NO);
    append_u16_be(init_raw, &pos, num_certificates);
    append_u16_be(init_raw, &pos, num_withdrawals);

    append_u8(init_raw, &pos, include_aux_data_hash ? ITEM_INCLUDED_YES : ITEM_INCLUDED_NO);

    if (include_aux_data_hash) {
        assert_true(aux_data_hash != NULL);
        assert_true(aux_data_hash_len == AUX_DATA_HASH_LENGTH);
        memcpy(init_raw + pos, aux_data_hash, AUX_DATA_HASH_LENGTH);
        pos += AUX_DATA_HASH_LENGTH;
    }

    append_u8(init_raw, &pos, include_validity_interval_start ? ITEM_INCLUDED_YES : ITEM_INCLUDED_NO);

    append_u16_be(init_raw, &pos, num_mint_asset_groups);

    append_u8(init_raw, &pos, ITEM_INCLUDED_NO);  // includeScriptDataHash
    append_u16_be(init_raw, &pos, 0);              // collateral inputs
    append_u16_be(init_raw, &pos, 0);              // required signers
    append_u8(init_raw, &pos, ITEM_INCLUDED_NO);   // includeNetworkId
    append_u8(init_raw, &pos, ITEM_INCLUDED_NO);   // includeCollateralOutput
    append_u8(init_raw, &pos, ITEM_INCLUDED_NO);   // includeTotalCollateral
    append_u16_be(init_raw, &pos, 0);              // reference inputs
    append_u16_be(init_raw, &pos, 0);              // voting procedures
    append_u8(init_raw, &pos, ITEM_INCLUDED_NO);   // includeTreasury
    append_u8(init_raw, &pos, ITEM_INCLUDED_NO);   // includeDonation

    append_u16_be(init_raw, &pos, num_witnesses);

    *out_len = pos;
}

static void run_shelley_test(const uint8_t *raw_tx,
                            size_t raw_tx_len,
                            const char *cbor_hex,
                            const char *expected_hash_hex,
                            uint16_t num_inputs,
                            uint16_t num_outputs,
                            uint16_t num_witnesses,
                            bool include_ttl,
                            bool include_validity_interval_start,
                            uint16_t num_certificates,
                            uint16_t num_withdrawals,
                            uint16_t num_mint_asset_groups,
                            bool include_aux_data_hash,
                            const char *aux_data_hash_hex,
                            uint64_t options) {
    reset_context();
    assert_true(app_mem_init());

    uint8_t init_raw[512];
    size_t init_len;
    uint8_t aux_data_hash[AUX_DATA_HASH_LENGTH] = {0};
    size_t aux_hash_len = 0;
    if (include_aux_data_hash) {
        aux_hash_len = hex_to_bytes(aux_data_hash_hex, aux_data_hash, sizeof(aux_data_hash));
        assert_true(aux_hash_len == AUX_DATA_HASH_LENGTH);
    }
    build_init_apdu(
        options,
        num_inputs,
        num_outputs,
        num_witnesses,
        include_ttl,
        include_validity_interval_start,
        num_certificates,
        num_withdrawals,
        num_mint_asset_groups,
        include_aux_data_hash,
        aux_hash_len ? aux_data_hash : NULL,
        aux_hash_len,
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
// Shelley Era Tests
// ======================================================================

static void test_sign_tx_without_outputs(void **state) {
    (void) state;
    run_shelley_test(FIXTURE_SHELLEY_SIGN_TX_WITHOUT_OUTPUTS_RAW_TX,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_OUTPUTS_RAW_TX_LEN,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_OUTPUTS_TX_BODY_CBOR_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_OUTPUTS_EXPECTED_HASH_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_OUTPUTS_NUM_INPUTS,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_OUTPUTS_NUM_OUTPUTS,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_OUTPUTS_NUM_WITNESSES,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_OUTPUTS_INCLUDE_TTL,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_OUTPUTS_INCLUDE_VALIDITY_INTERVAL_START,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_OUTPUTS_NUM_CERTIFICATES,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_OUTPUTS_NUM_WITHDRAWALS,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_OUTPUTS_NUM_MINT_ASSET_GROUPS,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_OUTPUTS_INCLUDE_AUX_DATA_HASH,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_OUTPUTS_AUX_DATA_HASH_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_OUTPUTS_OPTIONS);
}

static void test_sign_tx_with_258_tag_on_inputs(void **state) {
    (void) state;
    run_shelley_test(FIXTURE_SHELLEY_SIGN_TX_WITH_258_TAG_ON_INPUTS_RAW_TX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_258_TAG_ON_INPUTS_RAW_TX_LEN,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_258_TAG_ON_INPUTS_TX_BODY_CBOR_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_258_TAG_ON_INPUTS_EXPECTED_HASH_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_258_TAG_ON_INPUTS_NUM_INPUTS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_258_TAG_ON_INPUTS_NUM_OUTPUTS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_258_TAG_ON_INPUTS_NUM_WITNESSES,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_258_TAG_ON_INPUTS_INCLUDE_TTL,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_258_TAG_ON_INPUTS_INCLUDE_VALIDITY_INTERVAL_START,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_258_TAG_ON_INPUTS_NUM_CERTIFICATES,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_258_TAG_ON_INPUTS_NUM_WITHDRAWALS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_258_TAG_ON_INPUTS_NUM_MINT_ASSET_GROUPS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_258_TAG_ON_INPUTS_INCLUDE_AUX_DATA_HASH,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_258_TAG_ON_INPUTS_AUX_DATA_HASH_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_258_TAG_ON_INPUTS_OPTIONS);
}

static void test_sign_tx_without_change_address(void **state) {
    (void) state;
    run_shelley_test(FIXTURE_SHELLEY_SIGN_TX_WITHOUT_CHANGE_ADDRESS_RAW_TX,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_CHANGE_ADDRESS_RAW_TX_LEN,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_CHANGE_ADDRESS_TX_BODY_CBOR_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_CHANGE_ADDRESS_EXPECTED_HASH_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_CHANGE_ADDRESS_NUM_INPUTS,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_CHANGE_ADDRESS_NUM_OUTPUTS,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_CHANGE_ADDRESS_NUM_WITNESSES,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_CHANGE_ADDRESS_INCLUDE_TTL,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_CHANGE_ADDRESS_INCLUDE_VALIDITY_INTERVAL_START,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_CHANGE_ADDRESS_NUM_CERTIFICATES,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_CHANGE_ADDRESS_NUM_WITHDRAWALS,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_CHANGE_ADDRESS_NUM_MINT_ASSET_GROUPS,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_CHANGE_ADDRESS_INCLUDE_AUX_DATA_HASH,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_CHANGE_ADDRESS_AUX_DATA_HASH_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITHOUT_CHANGE_ADDRESS_OPTIONS);
}

static void test_sign_tx_with_change_base_address_with_staking_path(void **state) {
    (void) state;
    run_shelley_test(FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_PATH_RAW_TX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_PATH_RAW_TX_LEN,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_PATH_TX_BODY_CBOR_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_PATH_EXPECTED_HASH_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_PATH_NUM_INPUTS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_PATH_NUM_OUTPUTS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_PATH_NUM_WITNESSES,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_PATH_INCLUDE_TTL,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_PATH_INCLUDE_VALIDITY_INTERVAL_START,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_PATH_NUM_CERTIFICATES,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_PATH_NUM_WITHDRAWALS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_PATH_NUM_MINT_ASSET_GROUPS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_PATH_INCLUDE_AUX_DATA_HASH,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_PATH_AUX_DATA_HASH_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_PATH_OPTIONS);
}

static void test_sign_tx_with_change_base_address_with_staking_key_hash(void **state) {
    (void) state;
    run_shelley_test(FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_KEY_HASH_RAW_TX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_KEY_HASH_RAW_TX_LEN,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_KEY_HASH_TX_BODY_CBOR_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_KEY_HASH_EXPECTED_HASH_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_KEY_HASH_NUM_INPUTS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_KEY_HASH_NUM_OUTPUTS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_KEY_HASH_NUM_WITNESSES,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_KEY_HASH_INCLUDE_TTL,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_KEY_HASH_INCLUDE_VALIDITY_INTERVAL_START,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_KEY_HASH_NUM_CERTIFICATES,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_KEY_HASH_NUM_WITHDRAWALS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_KEY_HASH_NUM_MINT_ASSET_GROUPS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_KEY_HASH_INCLUDE_AUX_DATA_HASH,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_KEY_HASH_AUX_DATA_HASH_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_CHANGE_BASE_ADDRESS_WITH_STAKING_KEY_HASH_OPTIONS);
}

static void test_sign_tx_with_enterprise_change_address(void **state) {
    (void) state;
    run_shelley_test(FIXTURE_SHELLEY_SIGN_TX_WITH_ENTERPRISE_CHANGE_ADDRESS_RAW_TX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_ENTERPRISE_CHANGE_ADDRESS_RAW_TX_LEN,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_ENTERPRISE_CHANGE_ADDRESS_TX_BODY_CBOR_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_ENTERPRISE_CHANGE_ADDRESS_EXPECTED_HASH_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_ENTERPRISE_CHANGE_ADDRESS_NUM_INPUTS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_ENTERPRISE_CHANGE_ADDRESS_NUM_OUTPUTS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_ENTERPRISE_CHANGE_ADDRESS_NUM_WITNESSES,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_ENTERPRISE_CHANGE_ADDRESS_INCLUDE_TTL,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_ENTERPRISE_CHANGE_ADDRESS_INCLUDE_VALIDITY_INTERVAL_START,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_ENTERPRISE_CHANGE_ADDRESS_NUM_CERTIFICATES,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_ENTERPRISE_CHANGE_ADDRESS_NUM_WITHDRAWALS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_ENTERPRISE_CHANGE_ADDRESS_NUM_MINT_ASSET_GROUPS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_ENTERPRISE_CHANGE_ADDRESS_INCLUDE_AUX_DATA_HASH,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_ENTERPRISE_CHANGE_ADDRESS_AUX_DATA_HASH_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_ENTERPRISE_CHANGE_ADDRESS_OPTIONS);
}

static void test_sign_tx_with_pointer_change_address(void **state) {
    (void) state;
    run_shelley_test(FIXTURE_SHELLEY_SIGN_TX_WITH_POINTER_CHANGE_ADDRESS_RAW_TX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_POINTER_CHANGE_ADDRESS_RAW_TX_LEN,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_POINTER_CHANGE_ADDRESS_TX_BODY_CBOR_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_POINTER_CHANGE_ADDRESS_EXPECTED_HASH_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_POINTER_CHANGE_ADDRESS_NUM_INPUTS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_POINTER_CHANGE_ADDRESS_NUM_OUTPUTS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_POINTER_CHANGE_ADDRESS_NUM_WITNESSES,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_POINTER_CHANGE_ADDRESS_INCLUDE_TTL,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_POINTER_CHANGE_ADDRESS_INCLUDE_VALIDITY_INTERVAL_START,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_POINTER_CHANGE_ADDRESS_NUM_CERTIFICATES,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_POINTER_CHANGE_ADDRESS_NUM_WITHDRAWALS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_POINTER_CHANGE_ADDRESS_NUM_MINT_ASSET_GROUPS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_POINTER_CHANGE_ADDRESS_INCLUDE_AUX_DATA_HASH,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_POINTER_CHANGE_ADDRESS_AUX_DATA_HASH_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_POINTER_CHANGE_ADDRESS_OPTIONS);
}

static void test_sign_tx_with_non_reasonable_account_and_address(void **state) {
    (void) state;
    run_shelley_test(FIXTURE_SHELLEY_SIGN_TX_WITH_NON_REASONABLE_ACCOUNT_AND_ADDRESS_RAW_TX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_NON_REASONABLE_ACCOUNT_AND_ADDRESS_RAW_TX_LEN,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_NON_REASONABLE_ACCOUNT_AND_ADDRESS_TX_BODY_CBOR_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_NON_REASONABLE_ACCOUNT_AND_ADDRESS_EXPECTED_HASH_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_NON_REASONABLE_ACCOUNT_AND_ADDRESS_NUM_INPUTS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_NON_REASONABLE_ACCOUNT_AND_ADDRESS_NUM_OUTPUTS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_NON_REASONABLE_ACCOUNT_AND_ADDRESS_NUM_WITNESSES,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_NON_REASONABLE_ACCOUNT_AND_ADDRESS_INCLUDE_TTL,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_NON_REASONABLE_ACCOUNT_AND_ADDRESS_INCLUDE_VALIDITY_INTERVAL_START,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_NON_REASONABLE_ACCOUNT_AND_ADDRESS_NUM_CERTIFICATES,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_NON_REASONABLE_ACCOUNT_AND_ADDRESS_NUM_WITHDRAWALS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_NON_REASONABLE_ACCOUNT_AND_ADDRESS_NUM_MINT_ASSET_GROUPS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_NON_REASONABLE_ACCOUNT_AND_ADDRESS_INCLUDE_AUX_DATA_HASH,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_NON_REASONABLE_ACCOUNT_AND_ADDRESS_AUX_DATA_HASH_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_NON_REASONABLE_ACCOUNT_AND_ADDRESS_OPTIONS);
}

static void test_sign_tx_with_path_based_withdrawal(void **state) {
    (void) state;
    run_shelley_test(FIXTURE_SHELLEY_SIGN_TX_WITH_PATH_BASED_WITHDRAWAL_RAW_TX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_PATH_BASED_WITHDRAWAL_RAW_TX_LEN,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_PATH_BASED_WITHDRAWAL_TX_BODY_CBOR_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_PATH_BASED_WITHDRAWAL_EXPECTED_HASH_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_PATH_BASED_WITHDRAWAL_NUM_INPUTS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_PATH_BASED_WITHDRAWAL_NUM_OUTPUTS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_PATH_BASED_WITHDRAWAL_NUM_WITNESSES,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_PATH_BASED_WITHDRAWAL_INCLUDE_TTL,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_PATH_BASED_WITHDRAWAL_INCLUDE_VALIDITY_INTERVAL_START,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_PATH_BASED_WITHDRAWAL_NUM_CERTIFICATES,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_PATH_BASED_WITHDRAWAL_NUM_WITHDRAWALS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_PATH_BASED_WITHDRAWAL_NUM_MINT_ASSET_GROUPS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_PATH_BASED_WITHDRAWAL_INCLUDE_AUX_DATA_HASH,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_PATH_BASED_WITHDRAWAL_AUX_DATA_HASH_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_PATH_BASED_WITHDRAWAL_OPTIONS);
}

static void test_sign_tx_with_auxiliary_data_hash(void **state) {
    (void) state;
    run_shelley_test(FIXTURE_SHELLEY_SIGN_TX_WITH_AUXILIARY_DATA_HASH_RAW_TX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_AUXILIARY_DATA_HASH_RAW_TX_LEN,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_AUXILIARY_DATA_HASH_TX_BODY_CBOR_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_AUXILIARY_DATA_HASH_EXPECTED_HASH_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_AUXILIARY_DATA_HASH_NUM_INPUTS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_AUXILIARY_DATA_HASH_NUM_OUTPUTS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_AUXILIARY_DATA_HASH_NUM_WITNESSES,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_AUXILIARY_DATA_HASH_INCLUDE_TTL,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_AUXILIARY_DATA_HASH_INCLUDE_VALIDITY_INTERVAL_START,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_AUXILIARY_DATA_HASH_NUM_CERTIFICATES,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_AUXILIARY_DATA_HASH_NUM_WITHDRAWALS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_AUXILIARY_DATA_HASH_NUM_MINT_ASSET_GROUPS,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_AUXILIARY_DATA_HASH_INCLUDE_AUX_DATA_HASH,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_AUXILIARY_DATA_HASH_AUX_DATA_HASH_HEX,
                    FIXTURE_SHELLEY_SIGN_TX_WITH_AUXILIARY_DATA_HASH_OPTIONS);
}

// ======================================================================
// Main
// ======================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sign_tx_without_outputs),
        cmocka_unit_test(test_sign_tx_with_258_tag_on_inputs),
        cmocka_unit_test(test_sign_tx_without_change_address),
        cmocka_unit_test(test_sign_tx_with_change_base_address_with_staking_path),
        cmocka_unit_test(test_sign_tx_with_change_base_address_with_staking_key_hash),
        cmocka_unit_test(test_sign_tx_with_enterprise_change_address),
        cmocka_unit_test(test_sign_tx_with_pointer_change_address),
        cmocka_unit_test(test_sign_tx_with_non_reasonable_account_and_address),
        cmocka_unit_test(test_sign_tx_with_path_based_withdrawal),
        cmocka_unit_test(test_sign_tx_with_auxiliary_data_hash),
    };
    return _cmocka_run_group_tests("test_sign_tx_shelley", tests, ARRAY_LEN(tests), NULL, NULL);
}
