/*
 * Unit tests for src/ui/action/validate.c.
 *
 * validate.c turns the user's approve/reject choice into the right response:
 *   - validate_pubkey():      send pubkey on approve, status word on reject.
 *   - validate_transaction(): sign + send signature on approve (or a security
 *                             status word if signing fails), status word on
 *                             reject; the context state is updated accordingly.
 *
 * The signing syscall (bip32_derive_with_seed_ecdsa_sign_hash_256, the real
 * function behind the static-inline bip32_derive_ecdsa_sign_hash_256 wrapper),
 * the send_response helpers and io_send_sw are replaced with host stubs so we
 * can drive both the success and failure branches without a device.
 */

#include <string.h>

#include "unity.h"

#include "crypto_helpers.h"

#include "globals.h"
#include "sw.h"
#include "validate.h"
#include "send_response.h"

global_ctx_t G_context;

// ---- Controllable stubs ----

static uint16_t g_last_sw;
static int g_pubkey_resp_calls;
static int g_sig_resp_calls;

// Drives the signing result: return code and the sig_len it reports back.
static cx_err_t g_sign_ret;
static size_t g_sign_sig_len;

int io_send_response_buffers(const buffer_t *rdatalist, size_t count, uint16_t sw) {
    (void) rdatalist;
    (void) count;
    g_last_sw = sw;
    return (int) sw;
}

int helper_send_response_pubkey(void) {
    g_pubkey_resp_calls++;
    return 0;
}

int helper_send_response_sig(void) {
    g_sig_resp_calls++;
    return 0;
}

// Real (non-inline) function the bip32_derive_ecdsa_sign_hash_256 wrapper calls.
cx_err_t bip32_derive_with_seed_ecdsa_sign_hash_256(unsigned int derivation_mode,
                                                    cx_curve_t curve,
                                                    const uint32_t *path,
                                                    size_t path_len,
                                                    uint32_t sign_mode,
                                                    cx_md_t hashID,
                                                    const uint8_t *hash,
                                                    size_t hash_len,
                                                    uint8_t *sig,
                                                    size_t *sig_len,
                                                    uint32_t *info,
                                                    unsigned char *seed,
                                                    size_t seed_len) {
    (void) derivation_mode;
    (void) curve;
    (void) path;
    (void) path_len;
    (void) sign_mode;
    (void) hashID;
    (void) hash;
    (void) hash_len;
    (void) sig;
    (void) seed;
    (void) seed_len;
    if (sig_len != NULL) {
        *sig_len = g_sign_sig_len;
    }
    if (info != NULL) {
        *info = 0;
    }
    return g_sign_ret;
}

void setUp(void) {
    memset(&G_context, 0, sizeof(G_context));
    g_last_sw = 0;
    g_pubkey_resp_calls = 0;
    g_sig_resp_calls = 0;
    g_sign_ret = CX_OK;
    g_sign_sig_len = 70;
}

void tearDown(void) {
}

// =========================================================================
// validate_pubkey
// =========================================================================

void test_validate_pubkey_approve_sends_pubkey(void) {
    validate_pubkey(true);

    TEST_ASSERT_EQUAL(1, g_pubkey_resp_calls);
    TEST_ASSERT_EQUAL_HEX16(0, g_last_sw);  // no status word on approval
}

void test_validate_pubkey_reject_sends_status_word(void) {
    validate_pubkey(false);

    TEST_ASSERT_EQUAL(0, g_pubkey_resp_calls);
    TEST_ASSERT_EQUAL_HEX16(SWO_CONDITIONS_NOT_SATISFIED, g_last_sw);
}

// =========================================================================
// validate_transaction
// =========================================================================

void test_validate_transaction_approve_signs_and_sends_signature(void) {
    g_sign_ret = CX_OK;
    g_sign_sig_len = 70;

    validate_transaction(true);

    TEST_ASSERT_EQUAL(STATE_APPROVED, G_context.state);
    TEST_ASSERT_EQUAL(1, g_sig_resp_calls);
    TEST_ASSERT_EQUAL(70, G_context.tx_info.signature_len);
    TEST_ASSERT_EQUAL_HEX16(0, g_last_sw);  // no error status word
}

void test_validate_transaction_approve_sign_failure_sends_security_issue(void) {
    g_sign_ret = (cx_err_t) 1;  // anything but CX_OK

    validate_transaction(true);

    TEST_ASSERT_EQUAL(STATE_NONE, G_context.state);
    TEST_ASSERT_EQUAL(0, g_sig_resp_calls);
    TEST_ASSERT_EQUAL_HEX16(SWO_SECURITY_ISSUE, g_last_sw);
}

void test_validate_transaction_reject_sends_status_word(void) {
    validate_transaction(false);

    TEST_ASSERT_EQUAL(STATE_NONE, G_context.state);
    TEST_ASSERT_EQUAL(0, g_sig_resp_calls);
    TEST_ASSERT_EQUAL_HEX16(SWO_CONDITIONS_NOT_SATISFIED, g_last_sw);
}

// =========================================================================

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_validate_pubkey_approve_sends_pubkey);
    RUN_TEST(test_validate_pubkey_reject_sends_status_word);
    RUN_TEST(test_validate_transaction_approve_signs_and_sends_signature);
    RUN_TEST(test_validate_transaction_approve_sign_failure_sends_security_issue);
    RUN_TEST(test_validate_transaction_reject_sends_status_word);

    return UNITY_END();
}
