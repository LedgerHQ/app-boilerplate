#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <cmocka.h>

#include "utils/buffer_utils.h"
#include "utils/cbor.h"
#include "buffer.h"

static void test_buffer_write_u8_and_capacity(void **state) {
    (void) state;
    uint8_t raw[1] = {0};
    write_buffer_t buf = buffer_init(raw, sizeof(raw));

    assert_true(buffer_write_u8(&buf, 0xAB));
    assert_int_equal(raw[0], 0xAB);
    assert_int_equal(buf.offset, 1);

    // No space left, should fail without modifying offset
    assert_false(buffer_write_u8(&buf, 0xCD));
    assert_int_equal(buf.offset, 1);
}

static void test_buffer_write_u16_endianness(void **state) {
    (void) state;
    uint8_t raw[4] = {0};
    write_buffer_t buf = buffer_init(raw, sizeof(raw));

    assert_true(buffer_write_u16(&buf, 0x1234, BE));
    assert_int_equal(raw[0], 0x12);
    assert_int_equal(raw[1], 0x34);

    assert_true(buffer_write_u16(&buf, 0xABCD, LE));
    assert_int_equal(raw[2], 0xCD);
    assert_int_equal(raw[3], 0xAB);

    // Subsequent write must fail because buffer is full
    assert_false(buffer_write_u16(&buf, 0xFFFF, BE));
}

static void test_buffer_write_u32_u64(void **state) {
    (void) state;
    uint8_t raw[12] = {0};
    write_buffer_t buf = buffer_init(raw, sizeof(raw));

    assert_true(buffer_write_u32(&buf, 0xA1B2C3D4, BE));
    uint8_t expected_u32[] = {0xA1, 0xB2, 0xC3, 0xD4};
    assert_memory_equal(raw, expected_u32, sizeof(expected_u32));

    assert_true(buffer_write_u64(&buf, 0x0102030405060708ULL, LE));
    uint8_t expected_u64[] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};
    assert_memory_equal(raw + 4, expected_u64, sizeof(expected_u64));

    assert_false(buffer_write_u32(&buf, 0, BE));
}

static void test_buffer_write_bytes(void **state) {
    (void) state;
    uint8_t raw[5] = {0};
    write_buffer_t buf = buffer_init(raw, sizeof(raw));

    uint8_t first[] = {1, 2, 3};
    assert_true(buffer_write_bytes(&buf, first, sizeof(first)));
    assert_memory_equal(raw, first, sizeof(first));

    uint8_t second[] = {4, 5};
    assert_true(buffer_write_bytes(&buf, second, sizeof(second)));
    assert_memory_equal(raw + 3, second, sizeof(second));

    assert_false(buffer_write_bytes(&buf, (uint8_t[]){6}, 1));
}

static void test_buffer_write_cbor_token(void **state) {
    (void) state;
    uint8_t raw[16] = {0};
    write_buffer_t buf = buffer_init(raw, sizeof(raw));

    // Value fits into single byte
    assert_true(buffer_write_cbor_token(&buf, CBOR_TYPE_UNSIGNED, 0x15));
    assert_int_equal(raw[0], CBOR_TYPE_UNSIGNED | 0x15);
    assert_int_equal(buf.offset, 1);

    // Value requires additional bytes, but buffer still has space
    assert_true(buffer_write_cbor_token(&buf, CBOR_TYPE_UNSIGNED, 0x100));
    // First token was 1 byte, the second should occupy 3 bytes (major type + 0x19 + value)
    assert_int_equal(buf.offset, 4);

    // Force failure by limiting buffer size so cbor_writeToken cannot fit data
    write_buffer_t tiny = buffer_init(raw, 1);
    assert_false(buffer_write_cbor_token(&tiny, CBOR_TYPE_UNSIGNED, 0xFFFF));
    assert_int_equal(tiny.offset, 0);
}

static void test_buffer_read_bytes(void **state) {
    (void) state;
    uint8_t raw[] = {0xAA, 0xBB, 0xCC, 0xDD};
    buffer_t buf = {
        .ptr = raw,
        .size = sizeof(raw),
        .offset = 0,
    };

    uint8_t slice[2] = {0};
    assert_true(buffer_read_bytes(&buf, slice, sizeof(slice)));
    assert_memory_equal(slice, raw, sizeof(slice));
    assert_int_equal(buf.offset, 2);

    assert_true(buffer_read_bytes(&buf, slice, sizeof(slice)));
    assert_memory_equal(slice, raw + 2, sizeof(slice));
    assert_int_equal(buf.offset, 4);

    // Attempt to read past the end should fail and leave offset unchanged
    assert_false(buffer_read_bytes(&buf, slice, 1));
    assert_int_equal(buf.offset, 4);
}

static void test_buffer_read_bytes_ptr(void **state) {
    (void) state;
    uint8_t raw[] = {0xAA, 0xBB, 0xCC, 0xDD};
    buffer_t buf = {
        .ptr = raw,
        .size = sizeof(raw),
        .offset = 0,
    };

    uint8_t *slice = NULL;
    assert_true(buffer_read_bytes_ptr(&buf, &slice, 2));
    assert_ptr_equal(slice, raw);
    assert_int_equal(buf.offset, 2);

    assert_true(buffer_read_bytes_ptr(&buf, &slice, 2));
    assert_ptr_equal(slice, raw + 2);
    assert_int_equal(buf.offset, 4);

    // No bytes left in buffer
    assert_false(buffer_read_bytes_ptr(&buf, &slice, 1));
    assert_int_equal(buf.offset, 4);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_buffer_write_u8_and_capacity),
        cmocka_unit_test(test_buffer_write_u16_endianness),
        cmocka_unit_test(test_buffer_write_u32_u64),
        cmocka_unit_test(test_buffer_write_bytes),
        cmocka_unit_test(test_buffer_write_cbor_token),
        cmocka_unit_test(test_buffer_read_bytes),
        cmocka_unit_test(test_buffer_read_bytes_ptr),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
