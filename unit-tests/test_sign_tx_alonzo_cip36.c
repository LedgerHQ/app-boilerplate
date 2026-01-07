// Unit tests for Alonzo CIP36 transaction fixtures

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

#include "test_sign_tx_fixtures_alonzo_cip36.h"

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

static void test_sign_tx_with_cip36_registration_with_vote_key_hex_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_VOTE_KEY_HEX, false);
}

static void test_sign_tx_with_cip36_registration_with_vote_key_hex_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_VOTE_KEY_HEX, true);
}

static void test_sign_tx_with_cip36_registration_with_vote_key_path_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_VOTE_KEY_PATH, false);
}

static void test_sign_tx_with_cip36_registration_with_vote_key_path_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_VOTE_KEY_PATH, true);
}

static void test_sign_tx_with_cip36_registration_with_unusual_vote_key_path_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_UNUSUAL_VOTE_KEY_PATH, false);
}

static void test_sign_tx_with_cip36_registration_with_unusual_vote_key_path_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_UNUSUAL_VOTE_KEY_PATH, true);
}

static void test_sign_tx_with_cip36_registration_with_thirdparty_payment_address_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_THIRDPARTY_PAYMENT_ADDRESS, false);
}

static void test_sign_tx_with_cip36_registration_with_thirdparty_payment_address_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_THIRDPARTY_PAYMENT_ADDRESS, true);
}

static void test_sign_tx_with_cip36_registration_with_voting_purpose_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_VOTING_PURPOSE, false);
}

static void test_sign_tx_with_cip36_registration_with_voting_purpose_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_VOTING_PURPOSE, true);
}

static void test_sign_tx_with_cip36_registration_with_delegations_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_DELEGATIONS, false);
}

static void test_sign_tx_with_cip36_registration_with_delegations_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_DELEGATIONS, true);
}

// ======================================================================
// Main
// ======================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sign_tx_with_cip36_registration_with_vote_key_hex_expert_off),
        cmocka_unit_test(test_sign_tx_with_cip36_registration_with_vote_key_hex_expert_on),
        cmocka_unit_test(test_sign_tx_with_cip36_registration_with_vote_key_path_expert_off),
        cmocka_unit_test(test_sign_tx_with_cip36_registration_with_vote_key_path_expert_on),
        cmocka_unit_test(test_sign_tx_with_cip36_registration_with_unusual_vote_key_path_expert_off),
        cmocka_unit_test(test_sign_tx_with_cip36_registration_with_unusual_vote_key_path_expert_on),
        cmocka_unit_test(test_sign_tx_with_cip36_registration_with_thirdparty_payment_address_expert_off),
        cmocka_unit_test(test_sign_tx_with_cip36_registration_with_thirdparty_payment_address_expert_on),
        cmocka_unit_test(test_sign_tx_with_cip36_registration_with_voting_purpose_expert_off),
        cmocka_unit_test(test_sign_tx_with_cip36_registration_with_voting_purpose_expert_on),
        cmocka_unit_test(test_sign_tx_with_cip36_registration_with_delegations_expert_off),
        cmocka_unit_test(test_sign_tx_with_cip36_registration_with_delegations_expert_on),
    };
    return _cmocka_run_group_tests("test_sign_tx_alonzo_cip36", tests, ARRAY_LEN(tests), NULL, NULL);
}
