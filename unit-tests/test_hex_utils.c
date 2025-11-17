#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <cmocka.h>

#include "utils/hexUtils.h"

static void test_hex_nibble_parsing(void **state) {
    (void) state;

    // Valid nibble test vectors
    struct {
        char nibble;
        int value;
    } testVectors[] = {
        {'0', 0},  {'1', 1},  {'2', 2},  {'3', 3},  {'4', 4},  {'5', 5},
        {'6', 6},  {'7', 7},  {'8', 8},  {'9', 9},

        {'a', 10}, {'b', 11}, {'c', 12}, {'d', 13}, {'e', 14}, {'f', 15},

        {'A', 10}, {'B', 11}, {'C', 12}, {'D', 13}, {'E', 14}, {'F', 15},
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        uint8_t nibble;
        bool success = hex_parseNibble(testVectors[i].nibble, &nibble);
        assert_true(success);
        assert_int_equal(nibble, testVectors[i].value);
    }
}

static void test_hex_nibble_invalid(void **state) {
    (void) state;

    // Invalid nibble characters that should return false
    char invalidNibbles[] = {
        '\x00', '\x01', '.', '/', ':', ';', '?', '@', 'G', 'H', 'Z',
        '[', '\\', '_', '`', 'g', 'h', 'z', '{', 127, (char)128, (char)255
    };

    for (size_t i = 0; i < sizeof(invalidNibbles) / sizeof(invalidNibbles[0]); i++) {
        uint8_t nibble;
        bool success = hex_parseNibble(invalidNibbles[i], &nibble);
        assert_false(success);
    }
}

static void test_hex_parsing(void **state) {
    (void) state;

    struct {
        const char* hex;
        uint8_t raw;
    } testVectors[] = {
        {"ff", 0xff},
        {"00", 0x00},
        {"1a", 0x1a},
        {"2b", 0x2b},
        {"3c", 0x3c},
        {"4d", 0x4d},
        {"5f", 0x5f},
        {"98", 0x98},
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        uint8_t result;
        bool success = hex_parseNibblePair(testVectors[i].hex, &result);
        assert_true(success);
        assert_int_equal(result, testVectors[i].raw);
    }
}

static void test_decode_hex(void **state) {
    (void) state;

    // Test basic hex decoding
    const char* hexStr = "48656c6c6f";  // "Hello" in hex
    uint8_t buffer[10];
    size_t len;
    bool success = decode_hex(hexStr, buffer, sizeof(buffer), &len);

    assert_true(success);
    assert_int_equal(len, 5);
    assert_memory_equal(buffer, "Hello", 5);
}

static void test_encode_hex(void **state) {
    (void) state;

    // Test basic hex encoding
    const uint8_t bytes[] = {0x48, 0x65, 0x6c, 0x6c, 0x6f};  // "Hello"
    char hexStr[20];
    size_t len = encode_hex(bytes, sizeof(bytes), hexStr, sizeof(hexStr));

    assert_int_equal(len, 10);  // 5 bytes * 2 chars per byte
    assert_string_equal(hexStr, "48656c6c6f");
}

static void test_hex_roundtrip(void **state) {
    (void) state;

    // Test encode -> decode roundtrip
    const uint8_t original[] = {0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe};
    char hexStr[20];
    uint8_t decoded[10];

    size_t encLen = encode_hex(original, sizeof(original), hexStr, sizeof(hexStr));
    assert_int_equal(encLen, 12);

    size_t decLen;
    bool success = decode_hex(hexStr, decoded, sizeof(decoded), &decLen);
    assert_true(success);
    assert_int_equal(decLen, sizeof(original));
    assert_memory_equal(decoded, original, sizeof(original));
}

static void test_decode_hex_invalid(void **state) {
    (void) state;

    // Test decode with invalid hex characters
    const char* invalidHexStr = "4865xxc6";
    uint8_t buffer[10];
    size_t len;
    bool success = decode_hex(invalidHexStr, buffer, sizeof(buffer), &len);

    assert_false(success);
}

static void test_decode_hex_odd_length(void **state) {
    (void) state;

    // Test decode with odd length string
    const char* oddHexStr = "48656c6c6f0";
    uint8_t buffer[10];
    size_t len;
    bool success = decode_hex(oddHexStr, buffer, sizeof(buffer), &len);

    assert_false(success);
}

static void test_decode_hex_buffer_too_small(void **state) {
    (void) state;

    // Test decode with buffer too small
    const char* hexStr = "48656c6c6f";  // 5 bytes
    uint8_t buffer[3];
    size_t len;
    bool success = decode_hex(hexStr, buffer, sizeof(buffer), &len);

    assert_false(success);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_hex_nibble_parsing),
        cmocka_unit_test(test_hex_nibble_invalid),
        cmocka_unit_test(test_hex_parsing),
        cmocka_unit_test(test_decode_hex),
        cmocka_unit_test(test_encode_hex),
        cmocka_unit_test(test_hex_roundtrip),
        cmocka_unit_test(test_decode_hex_invalid),
        cmocka_unit_test(test_decode_hex_odd_length),
        cmocka_unit_test(test_decode_hex_buffer_too_small),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
