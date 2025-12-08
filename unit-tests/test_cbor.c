#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <cmocka.h>

#include "utils/cbor.h"
#include "hexUtils.h"

// Test vectors are taken from
// https://tools.ietf.org/html/rfc7049#appendix-A
static void test_cbor_parse_token(void **state) {
    (void) state;

    struct {
        const char* hex;
        uint8_t type;
        uint8_t width;
        uint64_t value;
    } testVectors[] = {
        {"00", CBOR_TYPE_UNSIGNED, 0, 0},
        {"01", CBOR_TYPE_UNSIGNED, 0, 1},
        {"0a", CBOR_TYPE_UNSIGNED, 0, 10},
        {"17", CBOR_TYPE_UNSIGNED, 0, 23},

        {"1818", CBOR_TYPE_UNSIGNED, 1, 24},

        {"1903e8", CBOR_TYPE_UNSIGNED, 2, 1000},

        {"1a000f4240", CBOR_TYPE_UNSIGNED, 4, 1000000},

        {"1b000000e8d4a51000", CBOR_TYPE_UNSIGNED, 8, 1000000000000},
        {"1bffFFffFFffFFffFF", CBOR_TYPE_UNSIGNED, 8, 18446744073709551615u},

        {"20", CBOR_TYPE_NEGATIVE, 0, -1},
        {"29", CBOR_TYPE_NEGATIVE, 0, -10},
        {"37", CBOR_TYPE_NEGATIVE, 0, -24},

        {"3818", CBOR_TYPE_NEGATIVE, 1, -25},
        {"38ff", CBOR_TYPE_NEGATIVE, 1, -256},

        {"390100", CBOR_TYPE_NEGATIVE, 2, -257},
        {"39ffff", CBOR_TYPE_NEGATIVE, 2, -65536},

        {"3a00010000", CBOR_TYPE_NEGATIVE, 4, -65537},
        {"3affffffff", CBOR_TYPE_NEGATIVE, 4, -4294967296},

        {"3b0000000100000000", CBOR_TYPE_NEGATIVE, 8, -4294967297},
        {"3b7FFFFFFFFFFFFFFF", CBOR_TYPE_NEGATIVE, 8, INT64_MIN},

        {"40", CBOR_TYPE_BYTES, 0, 0},
        {"44", CBOR_TYPE_BYTES, 0, 4},

        {"80", CBOR_TYPE_ARRAY, 0, 0},
        {"83", CBOR_TYPE_ARRAY, 0, 3},
        {"9819", CBOR_TYPE_ARRAY, 1, 25},

        {"9f", CBOR_TYPE_ARRAY_INDEF, 0, 0},

        {"a0", CBOR_TYPE_MAP, 0, 0},
        {"a1", CBOR_TYPE_MAP, 0, 1},

        {"d818", CBOR_TYPE_TAG, 1, 24},

        {"ff", CBOR_TYPE_INDEF_END, 0, 0},
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        uint8_t buf[20] = {0};
        size_t bufSize;
        bool success = decode_hex(testVectors[i].hex, buf, sizeof(buf), &bufSize);
        assert_true(success);

        cbor_token_t res = {0};
        bool parseSuccess = cbor_parseToken(buf, bufSize, &res);

        assert_true(parseSuccess);
        assert_int_equal(res.type, testVectors[i].type);
        assert_int_equal(res.width, testVectors[i].width);
        assert_int_equal(res.value, testVectors[i].value);
    }
}

// Test whether we reject non-canonical serialization
static void test_cbor_parse_noncanonical(void **state) {
    (void) state;

    struct {
        const char* hex;
    } testVectors[] = {
        {"1800"},
        {"1817"},

        {"190000"},
        {"1900ff"},

        {"1a00000000"},
        {"1a0000ffff"},

        {"1b0000000000000000"},
        {"1b00000000ffffffff"},
        // CBOR NEGATIVE type but smaller than INT64_MIN
        {"1cffFFffFFffFFffFF"},
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        uint8_t buf[20] = {0};
        size_t bufSize;
        bool success = decode_hex(testVectors[i].hex, buf, sizeof(buf), &bufSize);
        assert_true(success);

        cbor_token_t res = {0};
        bool parseSuccess = cbor_parseToken(buf, bufSize, &res);

        // Non-canonical serialization should be rejected
        assert_false(parseSuccess);
    }
}

static void test_cbor_write_token(void **state) {
    (void) state;

    struct {
        const char* hex;
        uint8_t type;
        uint64_t value;
    } testVectors[] = {
        {"00", CBOR_TYPE_UNSIGNED, 0},
        {"01", CBOR_TYPE_UNSIGNED, 1},
        {"0a", CBOR_TYPE_UNSIGNED, 10},
        {"17", CBOR_TYPE_UNSIGNED, 23},

        {"1818", CBOR_TYPE_UNSIGNED, 24},

        {"1903e8", CBOR_TYPE_UNSIGNED, 1000},

        {"1a000f4240", CBOR_TYPE_UNSIGNED, 1000000},

        {"1b000000e8d4a51000", CBOR_TYPE_UNSIGNED, 1000000000000},
        {"1bffFFffFFffFFffFF", CBOR_TYPE_UNSIGNED, 18446744073709551615u},

        // 0b0010 0000
        {"20", CBOR_TYPE_NEGATIVE, -1},
        // 0b0010 1001
        {"29", CBOR_TYPE_NEGATIVE, -10},

        // 0b0011 0111
        {"37", CBOR_TYPE_NEGATIVE, -24},
        // 0b0011 1000 == type | 24
        {"3818", CBOR_TYPE_NEGATIVE, -25},

        {"38ff", CBOR_TYPE_NEGATIVE, -256},
        // 0b0011 1001 == type | 25
        {"390100", CBOR_TYPE_NEGATIVE, -257},

        {"39ffff", CBOR_TYPE_NEGATIVE, -65536},
        // 0b0011 1010 == type | 26
        {"3a00010000", CBOR_TYPE_NEGATIVE, -65537},

        {"3affffffff", CBOR_TYPE_NEGATIVE, -4294967296},
        // 0b0011 1011 == type | 27
        {"3b0000000100000000", CBOR_TYPE_NEGATIVE, -4294967297},

        {"3b7FFFFFFFFFFFFFFF", CBOR_TYPE_NEGATIVE, INT64_MIN},

        {"40", CBOR_TYPE_BYTES, 0},
        {"44", CBOR_TYPE_BYTES, 4},

        {"80", CBOR_TYPE_ARRAY, 0},
        {"83", CBOR_TYPE_ARRAY, 3},
        {"9819", CBOR_TYPE_ARRAY, 25},

        {"9f", CBOR_TYPE_ARRAY_INDEF, 0},

        {"a0", CBOR_TYPE_MAP, 0},
        {"a1", CBOR_TYPE_MAP, 1},

        {"ff", CBOR_TYPE_INDEF_END, 0},
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        uint8_t expected[50] = {0};
        size_t expectedSize;
        bool success = decode_hex(testVectors[i].hex, expected, sizeof(expected), &expectedSize);
        assert_true(success);

        uint8_t buffer[50] = {0};
        size_t bufferSize = 0;
        bool writeSuccess = cbor_writeToken(testVectors[i].type, testVectors[i].value, buffer, sizeof(buffer), &bufferSize);

        assert_true(writeSuccess);
        assert_int_equal(bufferSize, expectedSize);
        assert_memory_equal(buffer, expected, expectedSize);
    }
}

// Test invalid types for writing
static void test_cbor_write_invalid_type(void **state) {
    (void) state;

    uint8_t buf[10] = {0};
    uint8_t invalid_types[] = {1, 2, 47};

    for (size_t i = 0; i < sizeof(invalid_types) / sizeof(invalid_types[0]); i++) {
        size_t out_size = 0;
        bool success = cbor_writeToken(invalid_types[i], 0, buf, sizeof(buf), &out_size);
        // Invalid types should return false
        assert_false(success);
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_cbor_parse_token),
        cmocka_unit_test(test_cbor_parse_noncanonical),
        cmocka_unit_test(test_cbor_write_token),
        cmocka_unit_test(test_cbor_write_invalid_type),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
