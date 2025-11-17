#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include <cmocka.h>

#include "utils/textUtils.h"

// Test str_formatDecimalAmount
static void test_format_decimal_basic(void **state) {
    (void) state;

    struct {
        uint64_t amount;
        size_t places;
        const char* expected;
    } testVectors[] = {
        {0, 0, "0"},
        {0, 4, "0.0000"},
        {1, 8, "0.00000001"},
        {10, 8, "0.00000010"},
        {123456, 4, "12.3456"},
        {1000000, 3, "1,000.000"},
        {12345678901234567890u, 12, "12,345,678.901234567890"},
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        char tmp[100] = {0};
        size_t len = str_formatDecimalAmount(
            testVectors[i].amount,
            testVectors[i].places,
            tmp,
            sizeof(tmp)
        );
        assert_int_equal(len, strlen(testVectors[i].expected));
        assert_string_equal(tmp, testVectors[i].expected);
    }
}

// Test str_formatAdaAmount
static void test_format_ada_basic(void **state) {
    (void) state;

    struct {
        uint64_t amount;
        const char* expected;
    } testVectors[] = {
        {0, "0.000000 ADA"},
        {1, "0.000001 ADA"},
        {10, "0.000010 ADA"},
        {123456, "0.123456 ADA"},
        {1000000, "1.000000 ADA"},
        {12345678901234567890u, "12,345,678,901,234.567890 ADA"},
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        char tmp[100] = {0};
        size_t len = str_formatAdaAmount(testVectors[i].amount, tmp, sizeof(tmp));
        assert_int_equal(len, strlen(testVectors[i].expected));
        assert_string_equal(tmp, testVectors[i].expected);
    }
}

// Test str_formatValidityBoundary (TTL/slot formatting)
static void test_format_validity_boundary(void **state) {
    (void) state;

    struct {
        uint64_t slotNumber;
        const char* expected;
    } testVectors[] = {
        // Byron era (slot numbers directly to epoch/slot)
        {123, "epoch 0 / slot 123"},
        {5 * 21600 + 124, "epoch 5 / slot 124"},
        // Shelley era (different slot duration)
        {4492800, "epoch 208 / slot 0"},
        {4924799, "epoch 208 / slot 431999"},
        {4924800, "epoch 209 / slot 0"},
        // Invalid/large slot numbers
        {1000001llu * 432000 + 124, "epoch more than 1000000"},
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        char tmp[100] = {0};
        size_t len = str_formatValidityBoundary(testVectors[i].slotNumber, tmp, sizeof(tmp));
        assert_int_equal(len, strlen(testVectors[i].expected));
        assert_string_equal(tmp, testVectors[i].expected);
    }
}

// Test str_formatUint64
static void test_format_uint64(void **state) {
    (void) state;

    struct {
        uint64_t number;
        const char* expected;
    } testVectors[] = {
        {0, "0"},
        {1, "1"},
        {4924800, "4924800"},
        {4924799, "4924799"},
        {(uint64_t)(-1ll), "18446744073709551615"},  // Max uint64
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        char tmp[100] = {0};
        size_t len = str_formatUint64(testVectors[i].number, tmp, sizeof(tmp));
        assert_int_equal(len, strlen(testVectors[i].expected));
        assert_string_equal(tmp, testVectors[i].expected);
    }
}

// Test str_formatInt64
static void test_format_int64(void **state) {
    (void) state;

    struct {
        int64_t number;
        const char* expected;
    } testVectors[] = {
        {0, "0"},
        {1, "1"},
        {4924800, "4924800"},
        {4924799, "4924799"},
        {-1ll, "-1"},
        {-922337203685477580, "-922337203685477580"},
        {INT64_MIN, "-9223372036854775808"},
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        char tmp[100] = {0};
        size_t len = str_formatInt64(testVectors[i].number, tmp, sizeof(tmp));
        assert_int_equal(len, strlen(testVectors[i].expected));
        assert_string_equal(tmp, testVectors[i].expected);
    }
}

// Test ASCII validation functions
static void test_is_printable_ascii(void **state) {
    (void) state;

    // Valid printable ASCII without spaces
    const uint8_t* valid_no_spaces = (const uint8_t*)"HelloWorld123";
    assert_true(str_isPrintableAsciiWithoutSpaces(valid_no_spaces, strlen((const char*)valid_no_spaces)));

    // Valid printable ASCII with spaces
    const uint8_t* valid_with_spaces = (const uint8_t*)"Hello World 123";
    assert_true(str_isPrintableAsciiWithSpaces(valid_with_spaces, strlen((const char*)valid_with_spaces)));

    // Invalid - has space but checking without spaces allowed
    assert_false(str_isPrintableAsciiWithoutSpaces(valid_with_spaces, strlen((const char*)valid_with_spaces)));

    // Invalid - non-ASCII character
    const uint8_t invalid_ascii[] = {0x48, 0x65, 0xff, 0x00};  // "He" + invalid byte
    assert_false(str_isPrintableAsciiWithoutSpaces(invalid_ascii, 3));
    assert_false(str_isPrintableAsciiWithSpaces(invalid_ascii, 3));
}

// Test unambiguous ASCII
static void test_is_unambiguous_ascii(void **state) {
    (void) state;

    // Valid unambiguous ASCII - alphanumeric only
    const uint8_t* valid = (const uint8_t*)"HelloWorld123abc";
    assert_true(str_isUnambiguousAscii(valid, strlen((const char*)valid)));

    // Valid - single spaces in middle allowed
    const uint8_t* with_space = (const uint8_t*)"Hello World";
    assert_true(str_isUnambiguousAscii(with_space, strlen((const char*)with_space)));

    // Valid - printable special characters allowed (hyphen, etc)
    const uint8_t* with_special = (const uint8_t*)"Hello-World";
    assert_true(str_isUnambiguousAscii(with_special, strlen((const char*)with_special)));

    // Invalid - has leading space
    const uint8_t* leading_space = (const uint8_t*)" Hello";
    assert_false(str_isUnambiguousAscii(leading_space, strlen((const char*)leading_space)));

    // Invalid - has trailing space
    const uint8_t* trailing_space = (const uint8_t*)"Hello ";
    assert_false(str_isUnambiguousAscii(trailing_space, strlen((const char*)trailing_space)));

    // Invalid - double space
    const uint8_t* double_space = (const uint8_t*)"Hello  World";
    assert_false(str_isUnambiguousAscii(double_space, strlen((const char*)double_space)));

    // Invalid - has non-ASCII
    const uint8_t non_ascii[] = {0x48, 0x65, 0xff, 0x00};
    assert_false(str_isUnambiguousAscii(non_ascii, 3));

    // Invalid - empty string
    assert_false(str_isUnambiguousAscii((const uint8_t*)"", 0));
}

// Test text to buffer conversion
static void test_text_to_buffer(void **state) {
    (void) state;

    // Simple ASCII text conversion
    uint8_t buffer[100] = {0};
    size_t len = str_textToBuffer("Hello", buffer, sizeof(buffer));
    assert_int_equal(len, 5);
    assert_memory_equal(buffer, (const uint8_t*)"Hello", 5);

    // Text with spaces
    memset(buffer, 0, sizeof(buffer));
    len = str_textToBuffer("Hello World", buffer, sizeof(buffer));
    assert_int_equal(len, 11);
    assert_memory_equal(buffer, (const uint8_t*)"Hello World", 11);

    // Empty string
    memset(buffer, 0, sizeof(buffer));
    len = str_textToBuffer("", buffer, sizeof(buffer));
    assert_int_equal(len, 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_format_decimal_basic),
        cmocka_unit_test(test_format_ada_basic),
        cmocka_unit_test(test_format_validity_boundary),
        cmocka_unit_test(test_format_uint64),
        cmocka_unit_test(test_format_int64),
        cmocka_unit_test(test_is_printable_ascii),
        cmocka_unit_test(test_is_unambiguous_ascii),
        cmocka_unit_test(test_text_to_buffer),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
