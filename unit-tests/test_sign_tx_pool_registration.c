// Unit tests for Pool Registration transaction signing

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

#include "test_sign_tx_fixtures_pool_registration.h"

// ======================================================================
// Simple mocks for IO and UI plumbing
// ======================================================================

static uint8_t g_last_response[TX_HASH_LENGTH];
static size_t g_last_response_len = 0;
static uint16_t g_last_response_sw = 0;

#include "test_sign_tx_common.h"

int io_send_response_pointer(const uint8_t *buffer, size_t bufferLength, uint16_t swo) {
    assert_true(bufferLength <= sizeof(g_last_response));
    memcpy(g_last_response, buffer, bufferLength);
    g_last_response_len = bufferLength;
    g_last_response_sw = swo;
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

// ======================================================================
// Helper functions
// ======================================================================

// ======================================================================
// POOL_REGISTRATION Era Tests
// ======================================================================

static void test_witness_valid_multiple_mixed_owners_all_relays_pool_registration_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_VALID_MULTIPLE_MIXED_OWNERS_ALL_RELAYS_POOL_REGISTRATION, false);
}

static void test_witness_valid_multiple_mixed_owners_all_relays_pool_registration_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_VALID_MULTIPLE_MIXED_OWNERS_ALL_RELAYS_POOL_REGISTRATION, true);
}

static void test_witness_valid_single_path_owner_ipv4_relay_pool_registration_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_VALID_SINGLE_PATH_OWNER_IPV4_RELAY_POOL_REGISTRATION, false);
}

static void test_witness_valid_single_path_owner_ipv4_relay_pool_registration_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_VALID_SINGLE_PATH_OWNER_IPV4_RELAY_POOL_REGISTRATION, true);
}

static void test_witness_valid_multiple_mixed_owners_ipv4_relay_pool_registration_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_VALID_MULTIPLE_MIXED_OWNERS_IPV4_RELAY_POOL_REGISTRATION, false);
}

static void test_witness_valid_multiple_mixed_owners_ipv4_relay_pool_registration_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_VALID_MULTIPLE_MIXED_OWNERS_IPV4_RELAY_POOL_REGISTRATION, true);
}

static void test_witness_valid_multiple_mixed_owners_mixed_ipv4_single_host_relays_pool_registration_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_VALID_MULTIPLE_MIXED_OWNERS_MIXED_IPV4_SINGLE_HOST_RELAYS_POOL_REGISTRATION, false);
}

static void test_witness_valid_multiple_mixed_owners_mixed_ipv4_single_host_relays_pool_registration_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_VALID_MULTIPLE_MIXED_OWNERS_MIXED_IPV4_SINGLE_HOST_RELAYS_POOL_REGISTRATION, true);
}

static void test_witness_valid_multiple_mixed_owners_mixed_ipv4_ipv6_relays_pool_registration_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_VALID_MULTIPLE_MIXED_OWNERS_MIXED_IPV4_IPV6_RELAYS_POOL_REGISTRATION, false);
}

static void test_witness_valid_multiple_mixed_owners_mixed_ipv4_ipv6_relays_pool_registration_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_VALID_MULTIPLE_MIXED_OWNERS_MIXED_IPV4_IPV6_RELAYS_POOL_REGISTRATION, true);
}

static void test_witness_valid_single_path_owner_no_relays_pool_registration_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_VALID_SINGLE_PATH_OWNER_NO_RELAYS_POOL_REGISTRATION, false);
}

static void test_witness_valid_single_path_owner_no_relays_pool_registration_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_VALID_SINGLE_PATH_OWNER_NO_RELAYS_POOL_REGISTRATION, true);
}

static void test_witness_pool_registration_with_no_metadata_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_POOL_REGISTRATION_WITH_NO_METADATA, false);
}

static void test_witness_pool_registration_with_no_metadata_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_POOL_REGISTRATION_WITH_NO_METADATA, true);
}

static void test_witness_pool_registration_without_outputs_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_POOL_REGISTRATION_WITHOUT_OUTPUTS, false);
}

static void test_witness_pool_registration_without_outputs_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_POOL_REGISTRATION_WITHOUT_OUTPUTS, true);
}

static void test_witness_pool_registration_as_operator_with_no_owners_and_no_relays_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_POOL_REGISTRATION_AS_OPERATOR_WITH_NO_OWNERS_AND_NO_RELAYS, false);
}

static void test_witness_pool_registration_as_operator_with_no_owners_and_no_relays_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_POOL_REGISTRATION_AS_OPERATOR_WITH_NO_OWNERS_AND_NO_RELAYS, true);
}

static void test_witness_pool_registration_as_operator_with_one_owner_and_no_relays_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_POOL_REGISTRATION_AS_OPERATOR_WITH_ONE_OWNER_AND_NO_RELAYS, false);
}

static void test_witness_pool_registration_as_operator_with_one_owner_and_no_relays_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_POOL_REGISTRATION_AS_OPERATOR_WITH_ONE_OWNER_AND_NO_RELAYS, true);
}

static void test_witness_pool_registration_as_operator_with_multiple_owners_and_all_relays_expert_off(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_POOL_REGISTRATION_AS_OPERATOR_WITH_MULTIPLE_OWNERS_AND_ALL_RELAYS, false);
}

static void test_witness_pool_registration_as_operator_with_multiple_owners_and_all_relays_expert_on(void **state) {
    (void) state;
    run_fixture_with_expert_mode(&FIXTURE_POOL_REGISTRATION_WITNESS_POOL_REGISTRATION_AS_OPERATOR_WITH_MULTIPLE_OWNERS_AND_ALL_RELAYS, true);
}

// ======================================================================
// Main
// ======================================================================

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_witness_valid_multiple_mixed_owners_all_relays_pool_registration_expert_off),
        cmocka_unit_test(test_witness_valid_multiple_mixed_owners_all_relays_pool_registration_expert_on),
        cmocka_unit_test(test_witness_valid_single_path_owner_ipv4_relay_pool_registration_expert_off),
        cmocka_unit_test(test_witness_valid_single_path_owner_ipv4_relay_pool_registration_expert_on),
        cmocka_unit_test(test_witness_valid_multiple_mixed_owners_ipv4_relay_pool_registration_expert_off),
        cmocka_unit_test(test_witness_valid_multiple_mixed_owners_ipv4_relay_pool_registration_expert_on),
        cmocka_unit_test(test_witness_valid_multiple_mixed_owners_mixed_ipv4_single_host_relays_pool_registration_expert_off),
        cmocka_unit_test(test_witness_valid_multiple_mixed_owners_mixed_ipv4_single_host_relays_pool_registration_expert_on),
        cmocka_unit_test(test_witness_valid_multiple_mixed_owners_mixed_ipv4_ipv6_relays_pool_registration_expert_off),
        cmocka_unit_test(test_witness_valid_multiple_mixed_owners_mixed_ipv4_ipv6_relays_pool_registration_expert_on),
        cmocka_unit_test(test_witness_valid_single_path_owner_no_relays_pool_registration_expert_off),
        cmocka_unit_test(test_witness_valid_single_path_owner_no_relays_pool_registration_expert_on),
        cmocka_unit_test(test_witness_pool_registration_with_no_metadata_expert_off),
        cmocka_unit_test(test_witness_pool_registration_with_no_metadata_expert_on),
        cmocka_unit_test(test_witness_pool_registration_without_outputs_expert_off),
        cmocka_unit_test(test_witness_pool_registration_without_outputs_expert_on),
        cmocka_unit_test(test_witness_pool_registration_as_operator_with_no_owners_and_no_relays_expert_off),
        cmocka_unit_test(test_witness_pool_registration_as_operator_with_no_owners_and_no_relays_expert_on),
        cmocka_unit_test(test_witness_pool_registration_as_operator_with_one_owner_and_no_relays_expert_off),
        cmocka_unit_test(test_witness_pool_registration_as_operator_with_one_owner_and_no_relays_expert_on),
        cmocka_unit_test(test_witness_pool_registration_as_operator_with_multiple_owners_and_all_relays_expert_off),
        cmocka_unit_test(test_witness_pool_registration_as_operator_with_multiple_owners_and_all_relays_expert_on),
    };
    return _cmocka_run_group_tests("test_sign_tx_pool_registration", tests, ARRAY_LEN(tests), NULL, NULL);
}
