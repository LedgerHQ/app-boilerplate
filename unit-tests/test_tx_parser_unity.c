/*
 * Unity port of test_tx_parser.c (originally cmocka).
 *
 * SUT: src/transaction/{deserialize,serialize}.c. Per our approach every
 * dependency is a CMock mock, never the real code:
 *   - buffer_read_u64 / buffer_seek_cur / buffer_read_varint (buffer.h)
 *   - write_u64_be (write.h), varint_size / varint_write (varint.h)
 *   - transaction_utils_check_encoding (transaction/utils.h)
 *
 * The buffer/write/varint mocks use CMock _Stub callbacks that faithfully
 * emulate the handful of operations the parser needs (big-endian read/write
 * over the caller's buffer, offset advance, 1-byte varint). This keeps the
 * round-trip and field assertions meaningful while linking no dependency code.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity.h"

#include "Mockbuffer.h"
#include "Mockwrite.h"
#include "Mockvarint.h"
#include "Mockutils.h"

#include "transaction/serialize.h"
#include "transaction/deserialize.h"
#include "types.h"

// ---- Knobs to drive the error paths from individual tests ----
static bool g_check_encoding_ret;       // transaction_utils_check_encoding result
static int g_varint_write_ret;          // varint_write result (<0 => write failure)
static bool g_varint_read_fail_huge;    // read_varint: fail AND report a huge memo_len

// ---- Faithful fakes for the mocked dependencies (no SDK code linked) ----

static bool buffer_read_u64_fake(buffer_t *buffer,
                                 uint64_t *value,
                                 endianness_t endianness,
                                 int cmock_num_calls) {
    (void) endianness;  // the parser always reads big-endian
    (void) cmock_num_calls;
    if (buffer->offset + 8 > buffer->size) {
        return false;
    }
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) {
        v = (v << 8) | buffer->ptr[buffer->offset + i];
    }
    *value = v;
    buffer->offset += 8;
    return true;
}

static bool buffer_seek_cur_fake(buffer_t *buffer, size_t offset, int cmock_num_calls) {
    (void) cmock_num_calls;
    if (buffer->offset + offset > buffer->size) {
        return false;
    }
    buffer->offset += offset;
    return true;
}

static bool buffer_read_varint_fake(buffer_t *buffer, uint64_t *value, int cmock_num_calls) {
    (void) cmock_num_calls;
    if (g_varint_read_fail_huge) {
        *value = (uint64_t) MAX_MEMO_LEN + 1;  // drive the MEMO_LENGTH_ERROR branch
        return false;
    }
    if (buffer->offset >= buffer->size) {
        return false;
    }
    uint8_t first = buffer->ptr[buffer->offset];
    if (first >= 0xFD) {
        return false;  // multi-byte varints not needed by these vectors
    }
    *value = first;
    buffer->offset += 1;
    return true;
}

static void write_u64_be_fake(uint8_t *ptr, size_t offset, uint64_t value, int cmock_num_calls) {
    (void) cmock_num_calls;
    for (int i = 0; i < 8; i++) {
        ptr[offset + i] = (uint8_t) (value >> (8 * (7 - i)));
    }
}

static uint8_t varint_size_fake(uint64_t value, int cmock_num_calls) {
    (void) cmock_num_calls;
    return (value < 0xFD) ? 1 : 3;
}

static int varint_write_fake(uint8_t *out, size_t offset, uint64_t value, int cmock_num_calls) {
    (void) cmock_num_calls;
    if (g_varint_write_ret < 0) {
        return g_varint_write_ret;  // simulate a write failure
    }
    out[offset] = (uint8_t) value;  // 1-byte varint for value < 0xFD
    return 1;
}

static bool check_encoding_fake(const uint8_t *memo, uint64_t memo_len, int cmock_num_calls) {
    (void) memo;
    (void) memo_len;
    (void) cmock_num_calls;
    return g_check_encoding_ret;
}

void setUp(void) {
    Mockbuffer_Init();
    Mockwrite_Init();
    Mockvarint_Init();
    Mockutils_Init();

    buffer_read_u64_Stub(buffer_read_u64_fake);
    buffer_seek_cur_Stub(buffer_seek_cur_fake);
    buffer_read_varint_Stub(buffer_read_varint_fake);
    write_u64_be_Stub(write_u64_be_fake);
    varint_size_Stub(varint_size_fake);
    varint_write_Stub(varint_write_fake);
    transaction_utils_check_encoding_Stub(check_encoding_fake);

    g_check_encoding_ret = true;
    g_varint_write_ret = 1;
    g_varint_read_fail_huge = false;
}

void tearDown(void) {
    Mockbuffer_Verify();
    Mockwrite_Verify();
    Mockvarint_Verify();
    Mockutils_Verify();

    Mockbuffer_Destroy();
    Mockwrite_Destroy();
    Mockvarint_Destroy();
    Mockutils_Destroy();
}

void test_tx_serialization(void) {
    transaction_t tx;
    // clang-format off
    uint8_t raw_tx[] = {
        // nonce (8)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
        // to (20)
        0x7a, 0xc3, 0x39, 0x97, 0x54, 0x4e, 0x31, 0x75,
        0xd2, 0x66, 0xbd, 0x02, 0x24, 0x39, 0xb2, 0x2c,
        0xdb, 0x16, 0x50, 0x8c,
        // value (8)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x08, 0x07,
        // memo length (varint: 241)
        0xf1,
        // memo (var: 241)
        0x54, 0x68, 0x65, 0x20, 0x54, 0x68, 0x65, 0x6f,
        0x72, 0x79, 0x20, 0x6f, 0x66, 0x20, 0x47, 0x72,
        0x6f, 0x75, 0x70, 0x73, 0x20, 0x69, 0x73, 0x20,
        0x61, 0x20, 0x62, 0x72, 0x61, 0x6e, 0x63, 0x68,
        0x20, 0x6f, 0x66, 0x20, 0x6d, 0x61, 0x74, 0x68,
        0x65, 0x6d, 0x61, 0x74, 0x69, 0x63, 0x73, 0x20,
        0x69, 0x6e, 0x20, 0x77, 0x68, 0x69, 0x63, 0x68,
        0x20, 0x6f, 0x6e, 0x65, 0x20, 0x64, 0x6f, 0x65,
        0x73, 0x20, 0x73, 0x6f, 0x6d, 0x65, 0x74, 0x68,
        0x69, 0x6e, 0x67, 0x20, 0x74, 0x6f, 0x20, 0x73,
        0x6f, 0x6d, 0x65, 0x74, 0x68, 0x69, 0x6e, 0x67,
        0x20, 0x61, 0x6e, 0x64, 0x20, 0x74, 0x68, 0x65,
        0x6e, 0x20, 0x63, 0x6f, 0x6d, 0x70, 0x61, 0x72,
        0x65, 0x73, 0x20, 0x74, 0x68, 0x65, 0x20, 0x72,
        0x65, 0x73, 0x75, 0x6c, 0x74, 0x20, 0x77, 0x69,
        0x74, 0x68, 0x20, 0x74, 0x68, 0x65, 0x20, 0x72,
        0x65, 0x73, 0x75, 0x6c, 0x74, 0x20, 0x6f, 0x62,
        0x74, 0x61, 0x69, 0x6e, 0x65, 0x64, 0x20, 0x66,
        0x72, 0x6f, 0x6d, 0x20, 0x64, 0x6f, 0x69, 0x6e,
        0x67, 0x20, 0x74, 0x68, 0x65, 0x20, 0x73, 0x61,
        0x6d, 0x65, 0x20, 0x74, 0x68, 0x69, 0x6e, 0x67,
        0x20, 0x74, 0x6f, 0x20, 0x73, 0x6f, 0x6d, 0x65,
        0x74, 0x68, 0x69, 0x6e, 0x67, 0x20, 0x65, 0x6c,
        0x73, 0x65, 0x2c, 0x20, 0x6f, 0x72, 0x20, 0x73,
        0x6f, 0x6d, 0x65, 0x74, 0x68, 0x69, 0x6e, 0x67,
        0x20, 0x65, 0x6c, 0x73, 0x65, 0x20, 0x74, 0x6f,
        0x20, 0x74, 0x68, 0x65, 0x20, 0x73, 0x61, 0x6d,
        0x65, 0x20, 0x74, 0x68, 0x69, 0x6e, 0x67, 0x2e,
        0x20, 0x4e, 0x65, 0x77, 0x6d, 0x61, 0x6e, 0x2c,
        0x20, 0x4a, 0x61, 0x6d, 0x65, 0x73, 0x20, 0x52,
        0x2e
    };
    // clang-format on

    buffer_t buf = {.ptr = raw_tx, .size = sizeof(raw_tx), .offset = 0};

    parser_status_e status = transaction_deserialize(&buf, &tx, false);
    TEST_ASSERT_EQUAL(PARSING_OK, status);

    uint8_t output[300];
    int length = transaction_serialize(&tx, output, sizeof(output));
    TEST_ASSERT_EQUAL(sizeof(raw_tx), length);
    TEST_ASSERT_EQUAL_MEMORY(raw_tx, output, sizeof(raw_tx));
}

void test_token_tx_serialization(void) {
    transaction_t tx;
    // clang-format off
    uint8_t raw_token_tx[] = {
        // nonce (8)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
        // to (20)
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22,
        0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
        0x12, 0x34, 0x56, 0x78,
        // token_address (32)
        0xca, 0xfe, 0xba, 0xbe, 0xde, 0xad, 0xbe, 0xef,
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        // value (8)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0xe8,
        // memo length (varint: 12)
        0x0c,
        // memo (12 bytes)
        0x54, 0x6f, 0x6b, 0x65, 0x6e, 0x20, 0x74, 0x72,
        0x61, 0x6e, 0x73, 0x66
    };
    // clang-format on

    buffer_t buf = {.ptr = raw_token_tx, .size = sizeof(raw_token_tx), .offset = 0};

    parser_status_e status = transaction_deserialize(&buf, &tx, true);

    TEST_ASSERT_EQUAL(PARSING_OK, status);
    TEST_ASSERT_EQUAL(2, tx.nonce);
    TEST_ASSERT_EQUAL(1000, tx.value);
    TEST_ASSERT_EQUAL(12, tx.memo_len);
    TEST_ASSERT_NOT_NULL(tx.token_address);

    uint8_t expected_token_addr[] = {0xca, 0xfe, 0xba, 0xbe, 0xde, 0xad, 0xbe, 0xef,
                                     0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                                     0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
                                     0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    TEST_ASSERT_EQUAL_MEMORY(expected_token_addr, tx.token_address, 32);
}

void test_token_tx_error_short_buffer(void) {
    transaction_t tx;
    // clang-format off
    uint8_t raw_token_tx[] = {
        // nonce (8)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
        // to (20)
        0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22,
        0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0x00,
        0x12, 0x34, 0x56, 0x78,
        // token_address (only 10 bytes instead of 32 - truncated)
        0xca, 0xfe, 0xba, 0xbe, 0xde, 0xad, 0xbe, 0xef,
        0x00, 0x11
    };
    // clang-format on

    buffer_t buf = {.ptr = raw_token_tx, .size = sizeof(raw_token_tx), .offset = 0};

    parser_status_e status = transaction_deserialize(&buf, &tx, true);

    TEST_ASSERT_EQUAL(TOKEN_ADDRESS_PARSING_ERROR, status);
}

// =========================================================================
// deserialize error paths
// =========================================================================

// A minimal well-formed non-token tx: nonce(8) + to(20) + value(8) +
// varint memo_len + memo. Helpers below truncate it to hit each error.
static uint8_t g_tx[8 + ADDRESS_LEN + 8 + 1 + 3] = {
    // nonce
    0, 0, 0, 0, 0, 0, 0, 1,
    // to (20)
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    // value
    0, 0, 0, 0, 0, 0, 0, 2,
    // memo_len = 3
    3,
    // memo "abc"
    'a', 'b', 'c'};

static parser_status_e deserialize_size(size_t size) {
    transaction_t tx;
    buffer_t buf = {.ptr = g_tx, .size = size, .offset = 0};
    return transaction_deserialize(&buf, &tx, false);
}

void test_deserialize_too_long(void) {
    transaction_t tx;
    buffer_t buf = {.ptr = g_tx, .size = MAX_TX_LEN + 1, .offset = 0};  // checked first
    TEST_ASSERT_EQUAL(WRONG_LENGTH_ERROR, transaction_deserialize(&buf, &tx, false));
}

void test_deserialize_nonce_error(void) {
    TEST_ASSERT_EQUAL(NONCE_PARSING_ERROR, deserialize_size(4));  // < 8 bytes
}

void test_deserialize_to_error(void) {
    TEST_ASSERT_EQUAL(TO_PARSING_ERROR, deserialize_size(8));  // nonce only
}

void test_deserialize_value_error(void) {
    TEST_ASSERT_EQUAL(VALUE_PARSING_ERROR, deserialize_size(8 + ADDRESS_LEN));  // no value
}

void test_deserialize_memo_length_error(void) {
    // Reachable only because the source uses `&&` (read fails AND memo_len too
    // big) instead of `||` -- flagged as a likely bug; covered via the knob.
    g_varint_read_fail_huge = true;
    TEST_ASSERT_EQUAL(MEMO_LENGTH_ERROR, deserialize_size(8 + ADDRESS_LEN + 8));
}

void test_deserialize_memo_error(void) {
    // Reaches the memo seek with memo_len=3 but no memo bytes available.
    TEST_ASSERT_EQUAL(MEMO_PARSING_ERROR, deserialize_size(8 + ADDRESS_LEN + 8 + 1));
}

void test_deserialize_encoding_error(void) {
    g_check_encoding_ret = false;
    TEST_ASSERT_EQUAL(MEMO_ENCODING_ERROR, deserialize_size(sizeof(g_tx)));
}

void test_deserialize_trailing_bytes(void) {
    // One extra byte after the memo -> offset != size at the end.
    transaction_t tx;
    uint8_t buf_bytes[sizeof(g_tx) + 1] = {0};
    memcpy(buf_bytes, g_tx, sizeof(g_tx));
    buffer_t buf = {.ptr = buf_bytes, .size = sizeof(buf_bytes), .offset = 0};
    TEST_ASSERT_EQUAL(WRONG_LENGTH_ERROR, transaction_deserialize(&buf, &tx, false));
}

// =========================================================================
// serialize error paths
// =========================================================================

static transaction_t make_tx(void) {
    static uint8_t to[ADDRESS_LEN];
    static uint8_t memo[3] = {'a', 'b', 'c'};
    transaction_t tx = {0};
    tx.nonce = 1;
    tx.value = 2;
    tx.memo_len = sizeof(memo);
    tx.to = to;
    tx.memo = memo;
    return tx;
}

void test_serialize_output_too_small(void) {
    transaction_t tx = make_tx();
    uint8_t out[10];  // < 8 + 20 + 8 + 1 + 3
    TEST_ASSERT_EQUAL(-1, transaction_serialize(&tx, out, sizeof(out)));
}

void test_serialize_varint_write_error(void) {
    transaction_t tx = make_tx();
    uint8_t out[300];
    g_varint_write_ret = -1;  // simulate varint_write failure
    TEST_ASSERT_EQUAL(-1, transaction_serialize(&tx, out, sizeof(out)));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_tx_serialization);
    RUN_TEST(test_token_tx_serialization);
    RUN_TEST(test_token_tx_error_short_buffer);

    RUN_TEST(test_deserialize_too_long);
    RUN_TEST(test_deserialize_nonce_error);
    RUN_TEST(test_deserialize_to_error);
    RUN_TEST(test_deserialize_value_error);
    RUN_TEST(test_deserialize_memo_length_error);
    RUN_TEST(test_deserialize_memo_error);
    RUN_TEST(test_deserialize_encoding_error);
    RUN_TEST(test_deserialize_trailing_bytes);

    RUN_TEST(test_serialize_output_too_small);
    RUN_TEST(test_serialize_varint_write_error);

    return UNITY_END();
}
