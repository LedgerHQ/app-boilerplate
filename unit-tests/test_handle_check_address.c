/*
 * Unit tests for src/swap/handle_check_address.c.
 *
 * swap_handle_check_address() derives an address from the BIP32 path in
 * params->address_parameters and sets params->result = 1 iff the hex form
 * matches params->address_to_check.
 *
 * Isolation:
 *   - buffer_read_u8 / buffer_read_bip32_path : CMock (buffer.h);
 *   - bip32_derive_get_pubkey_256 is a static-inline wrapper -> the real
 *     bip32_derive_with_seed_get_pubkey_256 is stubbed (CX_OK / error);
 *   - address_from_pubkey (app) : host stub;
 *   - format_hex : CMock (format.h), driven by a callback to produce a known
 *     hex string, so the match/no-match decision is fully controlled.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity.h"

#include "Mockbuffer.h"
#include "Mockformat.h"

#include "swap.h"             // check_address_parameters_t, swap_handle_check_address
#include "address.h"         // address_from_pubkey (stubbed)
#include "crypto_helpers.h"  // cx_err_t, CX_OK
#include "tx_types.h"        // ADDRESS_LEN

// 40 hex chars == ADDRESS_LEN * 2: the address our mocked format_hex "derives".
#define DERIVED_HEX "00112233445566778899aabbccddeeff00112233"
#define DERIVE_ERROR 0x1234  // any non-CX_OK value

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

bool address_from_pubkey(const uint8_t public_key[static 65], uint8_t *out, size_t out_len) {
    (void) public_key;
    memset(out, 0, out_len);  // content is irrelevant: format_hex is mocked
    return true;
}

// format_hex mock: always write DERIVED_HEX into the output buffer.
static int format_hex_cb(const uint8_t *in,
                         size_t in_len,
                         char *out,
                         size_t out_len,
                         int cmock_num_calls) {
    (void) in;
    (void) in_len;
    (void) cmock_num_calls;
    strncpy(out, DERIVED_HEX, out_len - 1);
    out[out_len - 1] = '\0';
    return (int) strlen(out);
}

void setUp(void) {
    Mockbuffer_Init();
    Mockformat_Init();
    g_derive_ret = CX_OK;
}

void tearDown(void) {
    Mockbuffer_Verify();
    Mockformat_Verify();
    Mockbuffer_Destroy();
    Mockformat_Destroy();
}

// Build params with a (dummy) derivation path payload.
static uint8_t g_addr_params[8] = {0x05, 0x80, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00};

static check_address_parameters_t make_params(char *address_to_check) {
    check_address_parameters_t params = {0};
    params.address_parameters = g_addr_params;
    params.address_parameters_length = sizeof(g_addr_params);
    params.address_to_check = address_to_check;
    params.result = 0xff;  // sentinel; the function must overwrite it
    return params;
}

// Queue the mocks consumed once the parameter checks pass (derive + format).
static void expect_derive_and_format(void) {
    buffer_read_u8_ExpectAnyArgsAndReturn(true);
    buffer_read_bip32_path_ExpectAnyArgsAndReturn(true);
    format_hex_AddCallback(format_hex_cb);
    format_hex_ExpectAnyArgsAndReturn(ADDRESS_LEN * 2);
}

// =========================================================================
// Early-out guards -> result = 0, no derivation
// =========================================================================

void test_null_address_parameters(void) {
    char addr[] = DERIVED_HEX;
    check_address_parameters_t params = make_params(addr);
    params.address_parameters = NULL;

    swap_handle_check_address(&params);

    TEST_ASSERT_EQUAL(0, params.result);
}

void test_null_address_to_check(void) {
    check_address_parameters_t params = make_params(NULL);

    swap_handle_check_address(&params);

    TEST_ASSERT_EQUAL(0, params.result);
}

void test_wrong_address_length(void) {
    char too_short[] = "deadbeef";  // not ADDRESS_LEN * 2 chars
    check_address_parameters_t params = make_params(too_short);

    swap_handle_check_address(&params);

    TEST_ASSERT_EQUAL(0, params.result);
}

// =========================================================================
// Derivation failure -> result = 0 (format_hex never reached)
// =========================================================================

void test_derivation_failure(void) {
    char addr[] = DERIVED_HEX;
    check_address_parameters_t params = make_params(addr);

    buffer_read_u8_ExpectAnyArgsAndReturn(true);
    buffer_read_bip32_path_ExpectAnyArgsAndReturn(true);
    g_derive_ret = (cx_err_t) DERIVE_ERROR;

    swap_handle_check_address(&params);

    TEST_ASSERT_EQUAL(0, params.result);
}

// =========================================================================
// Match / mismatch
// =========================================================================

void test_address_match(void) {
    char addr[] = DERIVED_HEX;  // same as what format_hex produces
    check_address_parameters_t params = make_params(addr);

    expect_derive_and_format();

    swap_handle_check_address(&params);

    TEST_ASSERT_EQUAL(1, params.result);
}

void test_address_mismatch(void) {
    char addr[] = "ffffffffffffffffffffffffffffffffffffffff";  // 40 chars, != DERIVED_HEX
    check_address_parameters_t params = make_params(addr);

    expect_derive_and_format();

    swap_handle_check_address(&params);

    TEST_ASSERT_EQUAL(0, params.result);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_null_address_parameters);
    RUN_TEST(test_null_address_to_check);
    RUN_TEST(test_wrong_address_length);
    RUN_TEST(test_derivation_failure);
    RUN_TEST(test_address_match);
    RUN_TEST(test_address_mismatch);

    return UNITY_END();
}
