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
#include "cardano_constants.h"
#include "init_apdu.h"

#include "test_sign_tx_fixtures_mary.h"

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


// ======================================================================
// Mary Era Tests
// ======================================================================

static void test_sign_tx_with_a_multiasset_output(void **state) {
    (void) state;
    run_fixture(&FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_OUTPUT);
}

static void test_sign_tx_with_a_complex_multiasset_output(void **state) {
    (void) state;
    run_fixture(&FIXTURE_MARY_SIGN_TX_WITH_A_COMPLEX_MULTIASSET_OUTPUT);
}

static void test_sign_tx_with_big_numbers(void **state) {
    (void) state;
    run_fixture(&FIXTURE_MARY_SIGN_TX_WITH_BIG_NUMBERS);
}

static void test_sign_tx_with_a_multiasset_change_output(void **state) {
    (void) state;
    run_fixture(&FIXTURE_MARY_SIGN_TX_WITH_A_MULTIASSET_CHANGE_OUTPUT);
}

static void test_sign_tx_with_zero_fee_ttl_and_validity_interval_start(void **state) {
    (void) state;
    run_fixture(&FIXTURE_MARY_SIGN_TX_WITH_ZERO_FEE_TTL_AND_VALIDITY_INTERVAL_START);
}

static void test_sign_tx_with_output_with_decimal_places(void **state) {
    (void) state;
    run_fixture(&FIXTURE_MARY_SIGN_TX_WITH_OUTPUT_WITH_DECIMAL_PLACES);
}

static void test_sign_tx_with_mint_fields_with_various_amounts(void **state) {
    (void) state;
    run_fixture(&FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_WITH_VARIOUS_AMOUNTS);
}

static void test_sign_tx_with_mint_with_decimal_places(void **state) {
    (void) state;
    run_fixture(&FIXTURE_MARY_SIGN_TX_WITH_MINT_WITH_DECIMAL_PLACES);
}

static void test_sign_tx_with_mint_fields_among_other_fields(void **state) {
    (void) state;
    run_fixture(&FIXTURE_MARY_SIGN_TX_WITH_MINT_FIELDS_AMONG_OTHER_FIELDS);
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
