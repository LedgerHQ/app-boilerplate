/*
 * Unit tests for src/helper/send_reponse.c.
 *
 * helper_send_response_pubkey() / _sig() serialise data from G_context into a
 * response buffer and send it. They call io_send_response_pointer(), a
 * static-inline wrapper, so the mockable symbol is io_send_response_buffers
 * (io_send_response_pointer(ptr, size, sw) == io_send_response_buffers(&{ptr,
 * size, 0}, 1, sw)). A capture callback lets us assert the exact bytes,
 * length and status word produced.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity.h"

#include "Mockio.h"

#include "globals.h"
#include "send_response.h"
#include "constants.h"
#include "sw.h"
#include "types.h"

// Value the mocked send returns; the helpers must forward it verbatim.
#define SEND_RET 0x1234

global_ctx_t G_context;

// Capture of what io_send_response_buffers received.
static uint8_t g_resp[260];
static size_t g_resp_size;
static size_t g_count;
static uint16_t g_sw;

static int send_cb(const buffer_t *rdatalist, size_t count, uint16_t sw, int cmock_num_calls) {
    (void) cmock_num_calls;
    g_count = count;
    g_sw = sw;
    g_resp_size = rdatalist->size;
    memcpy(g_resp, rdatalist->ptr, rdatalist->size);
    return SEND_RET;
}

void setUp(void) {
    Mockio_Init();
    memset(&G_context, 0, sizeof(G_context));
    memset(g_resp, 0, sizeof(g_resp));
    g_resp_size = 0;
    g_count = 0;
    g_sw = 0;
}

void tearDown(void) {
    Mockio_Verify();
    Mockio_Destroy();
}

// =========================================================================
// helper_send_response_pubkey
//   resp = PUBKEY_LEN | raw_public_key | CHAINCODE_LEN | chain_code
// =========================================================================

void test_send_response_pubkey(void) {
    for (size_t i = 0; i < PUBKEY_LEN; i++) {
        G_context.pk_info.raw_public_key[i] = (uint8_t) (0x10 + i);
    }
    for (size_t i = 0; i < CHAINCODE_LEN; i++) {
        G_context.pk_info.chain_code[i] = (uint8_t) (0xA0 + i);
    }

    io_send_response_buffers_AddCallback(send_cb);
    io_send_response_buffers_ExpectAnyArgsAndReturn(SEND_RET);

    int ret = helper_send_response_pubkey();

    TEST_ASSERT_EQUAL(SEND_RET, ret);
    TEST_ASSERT_EQUAL(1, g_count);
    TEST_ASSERT_EQUAL_HEX16(SWO_SUCCESS, g_sw);

    // Layout and length.
    TEST_ASSERT_EQUAL(1 + PUBKEY_LEN + 1 + CHAINCODE_LEN, g_resp_size);
    TEST_ASSERT_EQUAL(PUBKEY_LEN, g_resp[0]);
    TEST_ASSERT_EQUAL_MEMORY(G_context.pk_info.raw_public_key, g_resp + 1, PUBKEY_LEN);
    TEST_ASSERT_EQUAL(CHAINCODE_LEN, g_resp[1 + PUBKEY_LEN]);
    TEST_ASSERT_EQUAL_MEMORY(G_context.pk_info.chain_code, g_resp + 1 + PUBKEY_LEN + 1, CHAINCODE_LEN);
}

// =========================================================================
// helper_send_response_sig
//   resp = signature_len | signature | v
// =========================================================================

void test_send_response_sig(void) {
    const uint8_t sig_len = 10;
    G_context.tx_info.signature_len = sig_len;
    for (uint8_t i = 0; i < sig_len; i++) {
        G_context.tx_info.signature[i] = (uint8_t) (0x40 + i);
    }
    G_context.tx_info.v = 0x01;

    io_send_response_buffers_AddCallback(send_cb);
    io_send_response_buffers_ExpectAnyArgsAndReturn(SEND_RET);

    int ret = helper_send_response_sig();

    TEST_ASSERT_EQUAL(SEND_RET, ret);
    TEST_ASSERT_EQUAL(1, g_count);
    TEST_ASSERT_EQUAL_HEX16(SWO_SUCCESS, g_sw);

    TEST_ASSERT_EQUAL(1 + sig_len + 1, g_resp_size);
    TEST_ASSERT_EQUAL(sig_len, g_resp[0]);
    TEST_ASSERT_EQUAL_MEMORY(G_context.tx_info.signature, g_resp + 1, sig_len);
    TEST_ASSERT_EQUAL(G_context.tx_info.v, g_resp[1 + sig_len]);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_send_response_pubkey);
    RUN_TEST(test_send_response_sig);

    return UNITY_END();
}
