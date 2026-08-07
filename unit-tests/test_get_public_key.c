/*
 * Unit tests for src/handler/get_public_key.c.
 *
 * handler_get_public_key() parses a BIP32 path, derives the public key, then
 * either shows the address (display=true) or sends the key. Dependencies are
 * isolated:
 *   - buffer_read_u8 / buffer_read_bip32_path : CMock (buffer.h) to drive parse
 *     success/failure;
 *   - bip32_derive_get_pubkey_256 is a static-inline wrapper, so the real
 *     bip32_derive_with_seed_get_pubkey_256 is stubbed to drive CX_OK / error;
 *   - io_send_sw -> io_send_response_buffers : CMock (io.h);
 *   - ui_display_address / helper_send_response_pubkey : host stubs (app code we
 *     don't want to pull in), returning sentinels so we can tell which branch ran.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity.h"

#include "Mockio.h"
#include "Mockbuffer.h"

#include "get_public_key.h"
#include "globals.h"
#include "types.h"
#include "sw.h"
#include "crypto_helpers.h"  // cx_err_t, CX_OK

#define RET_SEND_PUBKEY 0x0055
#define RET_DISPLAY     0x0066
#define DERIVE_ERROR    0x1234  // any non-CX_OK value (fits in the io_send_sw uint16_t)

global_ctx_t G_context;

// ---- Stubs ----

static cx_err_t g_derive_ret;

cx_err_t bip32_derive_with_seed_get_pubkey_256(unsigned int derivation_mode,
                                               cx_curve_t curve,
                                               const uint32_t *path,
                                               size_t path_len,
                                               uint8_t raw_pubkey[static 65],
                                               uint8_t *chain_code,
                                               cx_md_t hashID,
                                               unsigned char *seed,
                                               size_t seed_len) {
    (void) derivation_mode;
    (void) curve;
    (void) path;
    (void) path_len;
    (void) raw_pubkey;
    (void) chain_code;
    (void) hashID;
    (void) seed;
    (void) seed_len;
    return g_derive_ret;
}

static int g_pubkey_resp_calls;
static int g_display_calls;

int helper_send_response_pubkey(void) {
    g_pubkey_resp_calls++;
    return RET_SEND_PUBKEY;
}

int ui_display_address(void) {
    g_display_calls++;
    return RET_DISPLAY;
}

void setUp(void) {
    Mockio_Init();
    Mockbuffer_Init();
    memset(&G_context, 0, sizeof(G_context));
    g_derive_ret = CX_OK;
    g_pubkey_resp_calls = 0;
    g_display_calls = 0;
}

void tearDown(void) {
    Mockio_Verify();
    Mockbuffer_Verify();
    Mockio_Destroy();
    Mockbuffer_Destroy();
}

// =========================================================================
// Parse failures -> SWO_WRONG_DATA_LENGTH
// =========================================================================

void test_get_public_key_bad_path_len(void) {
    buffer_t cdata = {0};

    // First read (path length) fails -> short-circuit, no derivation.
    buffer_read_u8_ExpectAnyArgsAndReturn(false);

    io_send_response_buffers_ExpectAndReturn(NULL, 0, SWO_WRONG_DATA_LENGTH, SWO_WRONG_DATA_LENGTH);

    TEST_ASSERT_EQUAL(SWO_WRONG_DATA_LENGTH, handler_get_public_key(&cdata, false));
    TEST_ASSERT_EQUAL(0, g_pubkey_resp_calls);
    TEST_ASSERT_EQUAL(0, g_display_calls);
}

void test_get_public_key_bad_path(void) {
    buffer_t cdata = {0};

    buffer_read_u8_ExpectAnyArgsAndReturn(true);
    buffer_read_bip32_path_ExpectAnyArgsAndReturn(false);
    io_send_response_buffers_ExpectAndReturn(NULL, 0, SWO_WRONG_DATA_LENGTH, SWO_WRONG_DATA_LENGTH);

    TEST_ASSERT_EQUAL(SWO_WRONG_DATA_LENGTH, handler_get_public_key(&cdata, false));
}

// =========================================================================
// Derivation failure -> io_send_sw(error)
// =========================================================================

void test_get_public_key_derivation_error(void) {
    buffer_t cdata = {0};

    buffer_read_u8_ExpectAnyArgsAndReturn(true);
    buffer_read_bip32_path_ExpectAnyArgsAndReturn(true);
    g_derive_ret = (cx_err_t) DERIVE_ERROR;
    io_send_response_buffers_ExpectAndReturn(NULL, 0, DERIVE_ERROR, DERIVE_ERROR);

    TEST_ASSERT_EQUAL(DERIVE_ERROR, handler_get_public_key(&cdata, false));
    TEST_ASSERT_EQUAL(0, g_pubkey_resp_calls);
    TEST_ASSERT_EQUAL(0, g_display_calls);
}

// =========================================================================
// Success: no display -> send pubkey ; display -> address review
// =========================================================================

void test_get_public_key_success_no_display(void) {
    buffer_t cdata = {0};

    buffer_read_u8_ExpectAnyArgsAndReturn(true);
    buffer_read_bip32_path_ExpectAnyArgsAndReturn(true);
    g_derive_ret = CX_OK;

    int ret = handler_get_public_key(&cdata, false);

    TEST_ASSERT_EQUAL(RET_SEND_PUBKEY, ret);
    TEST_ASSERT_EQUAL(1, g_pubkey_resp_calls);
    TEST_ASSERT_EQUAL(0, g_display_calls);
    // Context is initialised for an address confirmation.
    TEST_ASSERT_EQUAL(CONFIRM_ADDRESS, G_context.req_type);
}

void test_get_public_key_success_display(void) {
    buffer_t cdata = {0};

    buffer_read_u8_ExpectAnyArgsAndReturn(true);
    buffer_read_bip32_path_ExpectAnyArgsAndReturn(true);
    g_derive_ret = CX_OK;

    int ret = handler_get_public_key(&cdata, true);

    TEST_ASSERT_EQUAL(RET_DISPLAY, ret);
    TEST_ASSERT_EQUAL(1, g_display_calls);
    TEST_ASSERT_EQUAL(0, g_pubkey_resp_calls);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_get_public_key_bad_path_len);
    RUN_TEST(test_get_public_key_bad_path);
    RUN_TEST(test_get_public_key_derivation_error);
    RUN_TEST(test_get_public_key_success_no_display);
    RUN_TEST(test_get_public_key_success_display);

    return UNITY_END();
}
