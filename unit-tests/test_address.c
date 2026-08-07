/*
 * Unit tests for src/address.c.
 *
 * address_from_pubkey() = Keccak256(public_key[1..65])[12:32]. The Keccak
 * syscall is the only dependency; it is mocked (cx_keccak_256_hash() is a
 * static-inline wrapper around cx_keccak_256_hash_iovec(), so that is the
 * symbol CMock replaces). We drive the hash result and return code to cover
 * the success, buffer-too-small and crypto-failure paths.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity.h"

#include "Mockledger_assert_internals.h"
#include "Mocklcx_sha3.h"

#include "address.h"
#include "tx_types.h"  // ADDRESS_LEN

static const uint8_t PUBLIC_KEY[65] = {0x04, 0x01, 0x02, 0x03};

static const uint8_t FAKE_DIGEST[CX_KECCAK_256_SIZE] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
    0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15,
    0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};

void setUp(void) {
    Mockledger_assert_internals_Init();
    Mocklcx_sha3_Init();
}

void tearDown(void) {
    Mockledger_assert_internals_Verify();
    Mocklcx_sha3_Verify();

    Mockledger_assert_internals_Destroy();
    Mocklcx_sha3_Destroy();
}

// Mock behaviour: fill the caller's digest buffer and report success.
static cx_err_t keccak_ok_cb(const cx_iovec_t *iovec,
                             size_t iovec_len,
                             uint8_t *digest,
                             int cmock_num_calls) {
    (void) iovec;
    (void) iovec_len;
    (void) cmock_num_calls;
    memcpy(digest, FAKE_DIGEST, sizeof(FAKE_DIGEST));
    return CX_OK;
}

static jmp_buf g_assert_jmp;

static void assert_exit_cb(bool confirm, int cmock_num_calls) {
    (void) confirm;
    (void) cmock_num_calls;
    longjmp(g_assert_jmp, 1);
}

void test_address_from_pubkey_null_out(void) {
    assert_exit_AddCallback(assert_exit_cb);
    assert_exit_ExpectAnyArgs();

    if (setjmp(g_assert_jmp) == 0) {
        address_from_pubkey(PUBLIC_KEY, NULL, 0);
        TEST_FAIL_MESSAGE("address_from_pubkey should not return on NULL out");
    }
}

void test_address_from_pubkey_success(void) {
    cx_keccak_256_hash_iovec_AddCallback(keccak_ok_cb);
    cx_keccak_256_hash_iovec_ExpectAnyArgsAndReturn(CX_OK);

    uint8_t out[ADDRESS_LEN] = {0};
    bool ok = address_from_pubkey(PUBLIC_KEY, out, sizeof(out));

    TEST_ASSERT_TRUE(ok);
    // address = last 20 bytes of the 32-byte digest
    TEST_ASSERT_EQUAL_MEMORY(FAKE_DIGEST + (CX_KECCAK_256_SIZE - ADDRESS_LEN), out, ADDRESS_LEN);
}

void test_address_from_pubkey_buffer_too_small(void) {
    // out_len < ADDRESS_LEN must fail before any hashing (no mock expected).
    uint8_t out[ADDRESS_LEN - 1] = {0};
    bool ok = address_from_pubkey(PUBLIC_KEY, out, sizeof(out));

    TEST_ASSERT_FALSE(ok);
}

void test_address_from_pubkey_hash_failure(void) {
    cx_keccak_256_hash_iovec_ExpectAnyArgsAndReturn(CX_INVALID_PARAMETER);

    uint8_t out[ADDRESS_LEN] = {0};
    bool ok = address_from_pubkey(PUBLIC_KEY, out, sizeof(out));

    TEST_ASSERT_FALSE(ok);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_address_from_pubkey_null_out);
    RUN_TEST(test_address_from_pubkey_success);
    RUN_TEST(test_address_from_pubkey_buffer_too_small);
    RUN_TEST(test_address_from_pubkey_hash_failure);

    return UNITY_END();
}
