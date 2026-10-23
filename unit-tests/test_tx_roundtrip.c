/*
 * Round-trip tests for src/transaction/{deserialize,serialize}.c.
 *
 * No mock: the transaction code runs against the real SDK buffer, read, write
 * and varint helpers. Each raw transaction is built field by field, its varint
 * memo length encoded here independently of the SDK.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity.h"

#include "buffer.h"
#include "deserialize.h"
#include "serialize.h"
#include "types.h"

#define NONCE       1
#define VALUE       0x090807
#define TOKEN_NONCE 2
#define TOKEN_VALUE 1000

static const uint8_t TO[ADDRESS_LEN] = {0x7a, 0xc3, 0x39, 0x97, 0x54, 0x4e, 0x31, 0x75, 0xd2, 0x66,
                                        0xbd, 0x02, 0x24, 0x39, 0xb2, 0x2c, 0xdb, 0x16, 0x50, 0x8c};

static const uint8_t TOKEN_ADDRESS[TOKEN_ADDRESS_LEN] = {
    0xca, 0xfe, 0xba, 0xbe, 0xde, 0xad, 0xbe, 0xef, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

static uint8_t g_raw[MAX_TX_LEN];
static uint8_t g_out[MAX_TX_LEN];

void setUp(void) {
    memset(g_raw, 0, sizeof(g_raw));
    memset(g_out, 0, sizeof(g_out));
}

void tearDown(void) {
}

static size_t put_u64_be(uint8_t *out, uint64_t value) {
    for (size_t i = 0; i < sizeof(value); i++) {
        out[i] = (uint8_t) (value >> (8 * (sizeof(value) - 1 - i)));
    }
    return sizeof(value);
}

// Bitcoin-style varint, limited to the 1- and 3-byte forms a memo length needs.
static size_t put_varint(uint8_t *out, uint16_t value) {
    if (value < 0xFD) {
        out[0] = (uint8_t) value;
        return 1;
    }
    out[0] = 0xFD;
    out[1] = (uint8_t) value;
    out[2] = (uint8_t) (value >> 8);
    return 3;
}

// Fill memo with printable ASCII.
static void fill_memo(uint8_t *memo, size_t len) {
    for (size_t i = 0; i < len; i++) {
        memo[i] = (uint8_t) ('a' + i % 26);
    }
}

// Build nonce | to | [token address] | value | varint memo length | memo into g_raw.
static size_t build_tx(uint64_t nonce,
                       const uint8_t *token_address,
                       uint64_t value,
                       const uint8_t *memo,
                       uint16_t memo_len) {
    size_t offset = 0;
    offset += put_u64_be(g_raw + offset, nonce);
    memcpy(g_raw + offset, TO, sizeof(TO));
    offset += sizeof(TO);
    if (token_address != NULL) {
        memcpy(g_raw + offset, token_address, TOKEN_ADDRESS_LEN);
        offset += TOKEN_ADDRESS_LEN;
    }
    offset += put_u64_be(g_raw + offset, value);
    offset += put_varint(g_raw + offset, memo_len);
    memcpy(g_raw + offset, memo, memo_len);
    offset += memo_len;
    TEST_ASSERT_TRUE(offset <= sizeof(g_raw));
    return offset;
}

// Deserialize a native transaction, check its fields, serialize it back and compare.
static void check_roundtrip(uint16_t memo_len) {
    uint8_t memo[MAX_MEMO_LEN];
    fill_memo(memo, memo_len);
    size_t raw_len = build_tx(NONCE, NULL, VALUE, memo, memo_len);

    transaction_t tx;
    buffer_t buf = {.ptr = g_raw, .size = raw_len, .offset = 0};
    TEST_ASSERT_EQUAL(PARSING_OK, transaction_deserialize(&buf, &tx, false));

    TEST_ASSERT_EQUAL_UINT64(NONCE, tx.nonce);
    TEST_ASSERT_EQUAL_MEMORY(TO, tx.to, sizeof(TO));
    TEST_ASSERT_NULL(tx.token_address);
    TEST_ASSERT_EQUAL_UINT64(VALUE, tx.value);
    TEST_ASSERT_EQUAL_UINT64(memo_len, tx.memo_len);
    if (memo_len > 0) {
        TEST_ASSERT_EQUAL_MEMORY(memo, tx.memo, memo_len);
    }

    int out_len = transaction_serialize(&tx, g_out, sizeof(g_out));
    TEST_ASSERT_EQUAL_INT((int) raw_len, out_len);
    TEST_ASSERT_EQUAL_MEMORY(g_raw, g_out, raw_len);
}

void test_roundtrip_empty_memo(void) {
    check_roundtrip(0);
}

void test_roundtrip_one_byte_varint(void) {
    check_roundtrip(0xFC);  // largest memo length encoded on 1 byte
}

void test_roundtrip_three_byte_varint(void) {
    check_roundtrip(0xFD);  // smallest memo length encoded on 3 bytes
}

void test_roundtrip_max_memo(void) {
    check_roundtrip(MAX_MEMO_LEN);
}

void test_serialize_output_too_small(void) {
    uint8_t memo[12];
    fill_memo(memo, sizeof(memo));
    size_t raw_len = build_tx(NONCE, NULL, VALUE, memo, sizeof(memo));

    transaction_t tx;
    buffer_t buf = {.ptr = g_raw, .size = raw_len, .offset = 0};
    TEST_ASSERT_EQUAL(PARSING_OK, transaction_deserialize(&buf, &tx, false));

    TEST_ASSERT_EQUAL_INT(-1, transaction_serialize(&tx, g_out, raw_len - 1));
}

void test_token_tx(void) {
    uint8_t memo[12];
    fill_memo(memo, sizeof(memo));
    size_t raw_len = build_tx(TOKEN_NONCE, TOKEN_ADDRESS, TOKEN_VALUE, memo, sizeof(memo));

    transaction_t tx;
    buffer_t buf = {.ptr = g_raw, .size = raw_len, .offset = 0};
    TEST_ASSERT_EQUAL(PARSING_OK, transaction_deserialize(&buf, &tx, true));

    TEST_ASSERT_EQUAL_UINT64(TOKEN_NONCE, tx.nonce);
    TEST_ASSERT_EQUAL_MEMORY(TO, tx.to, sizeof(TO));
    TEST_ASSERT_NOT_NULL(tx.token_address);
    TEST_ASSERT_EQUAL_MEMORY(TOKEN_ADDRESS, tx.token_address, TOKEN_ADDRESS_LEN);
    TEST_ASSERT_EQUAL_UINT64(TOKEN_VALUE, tx.value);
    TEST_ASSERT_EQUAL_UINT64(sizeof(memo), tx.memo_len);
    TEST_ASSERT_EQUAL_MEMORY(memo, tx.memo, sizeof(memo));
}

void test_token_tx_truncated_token_address(void) {
    uint8_t memo[12];
    fill_memo(memo, sizeof(memo));
    build_tx(TOKEN_NONCE, TOKEN_ADDRESS, TOKEN_VALUE, memo, sizeof(memo));

    transaction_t tx;
    // Stop 10 bytes into the token address.
    buffer_t buf = {.ptr = g_raw, .size = 8 + ADDRESS_LEN + 10, .offset = 0};
    TEST_ASSERT_EQUAL(TOKEN_ADDRESS_PARSING_ERROR, transaction_deserialize(&buf, &tx, true));
}

void test_missing_memo_length(void) {
    uint8_t memo[12];
    fill_memo(memo, sizeof(memo));
    build_tx(NONCE, NULL, VALUE, memo, sizeof(memo));

    transaction_t tx;
    // Stop right after the value field.
    buffer_t buf = {.ptr = g_raw, .size = 8 + ADDRESS_LEN + 8, .offset = 0};
    TEST_ASSERT_EQUAL(MEMO_LENGTH_ERROR, transaction_deserialize(&buf, &tx, false));
}

void test_memo_too_long(void) {
    uint8_t memo[MAX_MEMO_LEN + 1];
    fill_memo(memo, sizeof(memo));
    size_t raw_len = build_tx(NONCE, NULL, VALUE, memo, sizeof(memo));

    transaction_t tx;
    buffer_t buf = {.ptr = g_raw, .size = raw_len, .offset = 0};
    TEST_ASSERT_EQUAL(MEMO_LENGTH_ERROR, transaction_deserialize(&buf, &tx, false));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_roundtrip_empty_memo);
    RUN_TEST(test_roundtrip_one_byte_varint);
    RUN_TEST(test_roundtrip_three_byte_varint);
    RUN_TEST(test_roundtrip_max_memo);
    RUN_TEST(test_serialize_output_too_small);
    RUN_TEST(test_missing_memo_length);
    RUN_TEST(test_memo_too_long);
    RUN_TEST(test_token_tx);
    RUN_TEST(test_token_tx_truncated_token_address);

    return UNITY_END();
}
