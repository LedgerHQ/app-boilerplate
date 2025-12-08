#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <cmocka.h>

#include "hash.h"
#include "hexUtils.h"

// Test blake2b_512 functions using the mocks
static void test_blake2b_512_empty(void **state) {
    (void) state;

    // Create context
    cx_blake2b_full_t ctx;
    memset(&ctx, 0, sizeof(cx_blake2b_full_t));
    ctx.header.algo = 0;
    ctx.header.counter = 0;
    ctx.output_len = 64;
    ctx.buffer_len = 0;
    ctx.total_len = 0;

    // Finalize with no data
    uint8_t output[64] = {0};
    cx_err_t result = cx_hash_no_throw(&ctx.header, 0x80000000, NULL, 0, output, 64);
    assert_int_equal(result, CX_OK);

    const uint8_t expected[64] = {
        0x78, 0x6a, 0x02, 0xf7, 0x42, 0x01, 0x59, 0x03, 0xc6, 0xc6, 0xfd, 0x85, 0x25, 0x52, 0xd2, 0x72,
        0x91, 0x2f, 0x47, 0x40, 0xe1, 0x58, 0x47, 0x61, 0x8a, 0x86, 0xe2, 0x17, 0xf7, 0x1f, 0x54, 0x19,
        0xd2, 0x5e, 0x10, 0x31, 0xaf, 0xee, 0x58, 0x53, 0x13, 0x89, 0x64, 0x44, 0x93, 0x4e, 0xb0, 0x4b,
        0x90, 0x3a, 0x68, 0x5b, 0x14, 0x48, 0xb7, 0x55, 0xd5, 0x6f, 0x70, 0x1a, 0xfe, 0x9b, 0xe2, 0xce
    };

    assert_memory_equal(output, expected, sizeof(output));
}

static void test_blake2b_512_single_byte(void **state) {
    (void) state;

    cx_blake2b_full_t ctx;
    memset(&ctx, 0, sizeof(cx_blake2b_full_t));
    cx_blake2b_init_no_throw((cx_blake2b_t*)&ctx, 64);

    uint8_t input[1] = {0x52};
    cx_err_t result = cx_hash_no_throw(&ctx.header, 0, input, 1, NULL, 0);
    assert_int_equal(result, CX_OK);

    uint8_t output[64] = {0};
    result = cx_hash_no_throw(&ctx.header, 0x80000000, NULL, 0, output, 64);
    assert_int_equal(result, CX_OK);

    const uint8_t expected[64] = {
        0x32, 0x9a, 0x58, 0x3d, 0x5c, 0x11, 0xe0, 0x3e, 0x3e, 0x1d, 0xf9, 0x66, 0xdb, 0xb2, 0x8b, 0xee,
        0xa3, 0x95, 0xe0, 0x13, 0xbb, 0xcc, 0x75, 0x2e, 0x9f, 0xde, 0xc4, 0x0a, 0xb6, 0x61, 0xdd, 0x1e,
        0x5e, 0x5e, 0x4e, 0xd0, 0xcf, 0xf7, 0xb4, 0x12, 0x5d, 0x64, 0xb3, 0xbc, 0x7d, 0xa4, 0x76, 0x24,
        0x15, 0x29, 0x6d, 0x3b, 0x5e, 0x96, 0x4d, 0x78, 0x0f, 0xe8, 0x42, 0xa4, 0xcf, 0xca, 0x71, 0xb0
    };

    assert_memory_equal(output, expected, sizeof(output));
}

static void test_blake2b_512_chunked(void **state) {
    (void) state;

    cx_blake2b_full_t ctx;
    memset(&ctx, 0, sizeof(cx_blake2b_full_t));
    cx_blake2b_init_no_throw((cx_blake2b_t*)&ctx, 64);

    const char* chunks[] = {"52", "", "31", "", "ab4652", "eaeaea25", "36", "9217"};

    for (size_t i = 0; i < sizeof(chunks) / sizeof(chunks[0]); i++) {
        uint8_t chunk_buffer[32] = {0};
        size_t chunk_size;
        bool success = decode_hex(chunks[i], chunk_buffer, sizeof(chunk_buffer), &chunk_size);
        assert_true(success);
        if (chunk_size == 0) {
            continue;
        }
        cx_hash_no_throw(&ctx.header, 0, chunk_buffer, chunk_size, NULL, 0);
    }

    uint8_t output[64] = {0};
    cx_hash_no_throw(&ctx.header, 0x80000000, NULL, 0, output, 64);

    const uint8_t expected[64] = {
        0xf9, 0xc3, 0xaa, 0x38, 0xdd, 0x69, 0xd2, 0x69, 0x2f, 0x24, 0x58, 0x4d, 0xa9, 0x06, 0x01, 0x4a,
        0x08, 0xd8, 0xc7, 0x5c, 0xc7, 0x64, 0x47, 0x7e, 0x46, 0x47, 0x60, 0x74, 0xea, 0x96, 0xbf, 0x18,
        0x1b, 0xb1, 0xd7, 0x05, 0xce, 0x2c, 0x4c, 0xd9, 0x01, 0xc8, 0xf0, 0xf3, 0x8e, 0xf0, 0x3c, 0x47,
        0x97, 0x1a, 0x27, 0x5c, 0x1a, 0x9a, 0x58, 0x49, 0x50, 0x60, 0xd6, 0xae, 0xa5, 0xc7, 0x24, 0x81
    };

    assert_memory_equal(output, expected, sizeof(output));
}

static void test_blake2b_224_long_input(void **state) {
    (void) state;

    const char* input_hex =
        "5e4b43b19363f6e314819542d6e4ee1c383dd62dbc94c86a8634391e3b120b38"
        "8c49810e638fcc359444637e9679137b463bfd1126bcdb6f877f3c90a57c353a"
        "9ecb26b26e5bf15e58c37b83b63b01fc8ec1082f6624f998241e2dd11f3cc2b7"
        "e7e4af2ceb822f11f02ad7fb2aa2822f880da89ea825e0557708def47ea3d88a";

    uint8_t input[200] = {0};
    size_t input_size;
    bool success = decode_hex(input_hex, input, sizeof(input), &input_size);
    assert_true(success);

    cx_blake2b_full_t ctx;
    memset(&ctx, 0, sizeof(cx_blake2b_full_t));
    cx_blake2b_init_no_throw((cx_blake2b_t*)&ctx, 28);

    cx_hash_no_throw(&ctx.header, 0, input, input_size, NULL, 0);

    uint8_t output[28] = {0};
    cx_hash_no_throw(&ctx.header, 0x80000000, NULL, 0, output, 28);

    const uint8_t expected[28] = {
        0x8f, 0x91, 0x53, 0xcd, 0x38, 0xd4, 0x6d, 0x90, 0xd4, 0xe8, 0x8a, 0x77, 0x01, 0xaf, 0x5f, 0x9f,
        0xdd, 0xb6, 0x72, 0xd7, 0x0c, 0x2c, 0xe6, 0xdc, 0x5f, 0xac, 0xe6, 0xe3
    };

    assert_memory_equal(output, expected, sizeof(output));
}

static void test_blake2b_160_short_input(void **state) {
    (void) state;

    const char* input_hex = "7eae28af2208be856f7a119668ae52a49b73725e326dc16579dcc373";

    uint8_t input[200] = {0};
    size_t input_size;
    bool success = decode_hex(input_hex, input, sizeof(input), &input_size);
    assert_true(success);

    cx_blake2b_full_t ctx;
    memset(&ctx, 0, sizeof(cx_blake2b_full_t));
    cx_blake2b_init_no_throw((cx_blake2b_t*)&ctx, 20);

    cx_hash_no_throw(&ctx.header, 0, input, input_size, NULL, 0);

    uint8_t output[20] = {0};
    cx_hash_no_throw(&ctx.header, 0x80000000, NULL, 0, output, 20);

    const uint8_t expected[20] = {
        0x1c, 0xad, 0xfc, 0x0e, 0x70, 0x68, 0x80, 0x1d, 0x51, 0xd2, 0x40, 0xd1, 0x4a, 0x40, 0x85, 0xf2,
        0xa3, 0x67, 0x3c, 0xbb
    };

    assert_memory_equal(output, expected, sizeof(output));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_blake2b_512_empty),
        cmocka_unit_test(test_blake2b_512_single_byte),
        cmocka_unit_test(test_blake2b_512_chunked),
        cmocka_unit_test(test_blake2b_224_long_input),
        cmocka_unit_test(test_blake2b_160_short_input),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
