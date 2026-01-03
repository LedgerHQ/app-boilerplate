// Unit tests for Shelley era with certificates (stake delegation, registration)

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

#include "test_sign_tx_fixtures_shelley_certificates.h"

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
// SHELLEY_CERTIFICATES Era Tests
// ======================================================================

static void test_sign_tx_with_a_stake_registration_path_certificate_preconway(void **state) {
    (void) state;
    run_fixture(&FIXTURE_SHELLEY_CERTIFICATES_SIGN_TX_WITH_A_STAKE_REGISTRATION_PATH_CERTIFICATE_PRECONWAY);
}

static void test_sign_tx_with_a_stake_deregistration_path_certificate_preconway(void **state) {
    (void) state;
    run_fixture(&FIXTURE_SHELLEY_CERTIFICATES_SIGN_TX_WITH_A_STAKE_DEREGISTRATION_PATH_CERTIFICATE_PRECONWAY);
}

static void test_sign_tx_with_a_stake_delegation_path_certificate(void **state) {
    (void) state;
    run_fixture(&FIXTURE_SHELLEY_CERTIFICATES_SIGN_TX_WITH_A_STAKE_DELEGATION_PATH_CERTIFICATE);
}

static void test_sign_tx_and_filter_out_witnesses_with_duplicate_paths(void **state) {
    (void) state;
    run_fixture(&FIXTURE_SHELLEY_CERTIFICATES_SIGN_TX_AND_FILTER_OUT_WITNESSES_WITH_DUPLICATE_PATHS);
}

static void test_sign_tx_with_pool_retirement_combined_with_stake_registration(void **state) {
    (void) state;
    run_fixture(&FIXTURE_SHELLEY_CERTIFICATES_SIGN_TX_WITH_POOL_RETIREMENT_COMBINED_WITH_STAKE_REGISTRATION);
}

static void test_sign_tx_with_pool_retirement_combined_with_stake_deregistration(void **state) {
    (void) state;
    run_fixture(&FIXTURE_SHELLEY_CERTIFICATES_SIGN_TX_WITH_POOL_RETIREMENT_COMBINED_WITH_STAKE_DEREGISTRATION);
}

// ======================================================================
// Main
// ======================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_sign_tx_with_a_stake_registration_path_certificate_preconway),
        cmocka_unit_test(test_sign_tx_with_a_stake_deregistration_path_certificate_preconway),
        cmocka_unit_test(test_sign_tx_with_a_stake_delegation_path_certificate),
        cmocka_unit_test(test_sign_tx_and_filter_out_witnesses_with_duplicate_paths),
        cmocka_unit_test(test_sign_tx_with_pool_retirement_combined_with_stake_registration),
        cmocka_unit_test(test_sign_tx_with_pool_retirement_combined_with_stake_deregistration),
    };
    return _cmocka_run_group_tests("test_sign_tx_shelley_certificates", tests, ARRAY_LEN(tests), NULL, NULL);
}
