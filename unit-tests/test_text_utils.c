#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include <cmocka.h>

#include "utils/textUtils.h"
#include "utils/ipUtils.h"
#include "cardano_constants.h"

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

// Test str_formatValidityBoundaryMainnet (mainnet-specific TTL/slot formatting)
static void test_format_validity_boundary_mainnet(void **state) {
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
        size_t len = str_formatValidityBoundaryMainnet(testVectors[i].slotNumber, tmp, sizeof(tmp));
        assert_int_equal(len, strlen(testVectors[i].expected));
        assert_string_equal(tmp, testVectors[i].expected);
    }
}

// Test str_formatValidityBoundary (TTL/slot formatting with network detection)
static void test_format_validity_boundary(void **state) {
    (void) state;

    // Test mainnet (should use pretty formatting)
    {
        char tmp[100] = {0};
        size_t len = str_formatValidityBoundary(4492800, MAINNET_NETWORK_ID, MAINNET_PROTOCOL_MAGIC, tmp, sizeof(tmp));
        assert_string_equal(tmp, "epoch 208 / slot 0");
        assert_int_equal(len, strlen("epoch 208 / slot 0"));
    }

    // Test testnet (should use simple uint64 formatting)
    {
        char tmp[100] = {0};
        size_t len = str_formatValidityBoundary(12345, TESTNET_NETWORK_ID, TESTNET_PROTOCOL_MAGIC_LEGACY, tmp, sizeof(tmp));
        assert_string_equal(tmp, "12345");
        assert_int_equal(len, strlen("12345"));
    }

    // Test mainnet with wrong protocol magic (should use simple formatting)
    {
        char tmp[100] = {0};
        size_t len = str_formatValidityBoundary(12345, MAINNET_NETWORK_ID, TESTNET_PROTOCOL_MAGIC_LEGACY, tmp, sizeof(tmp));
        assert_string_equal(tmp, "12345");
        assert_int_equal(len, strlen("12345"));
    }
}

// Test abs_int64
static void test_abs_int64(void **state) {
    (void) state;

    struct {
        int64_t input;
        uint64_t expected;
    } testVectors[] = {
        {0, 0},
        {1, 1},
        {-1, 1},
        {123456, 123456},
        {-123456, 123456},
        {INT64_MAX, (uint64_t)INT64_MAX},
        {INT64_MIN, (uint64_t)INT64_MAX + 1},  // Special case: INT64_MIN
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        uint64_t result = abs_int64(testVectors[i].input);
        assert_int_equal(result, testVectors[i].expected);
    }
}

// Test format_u64
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
        explicit_bzero(tmp, sizeof(tmp));
        format_u64(tmp, sizeof(tmp), testVectors[i].number);
        assert_string_equal(tmp, testVectors[i].expected);
    }
}

// Test format_i64
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
        explicit_bzero(tmp, sizeof(tmp));
        format_i64(tmp, sizeof(tmp), testVectors[i].number);
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

// Test IPv4 address formatting
static void test_format_ipv4(void **state) {
    (void) state;

    char tmp[IPV4_STR_SIZE_MAX + 1] = {0};

    // IPv4 null case
    ipv4_t ipv4_null = {.isNull = true, .ip = {0, 0, 0, 0}};
    size_t len = str_formatIpv4(&ipv4_null, tmp, sizeof(tmp));
    assert_int_equal(len, strlen("(none)"));
    assert_string_equal(tmp, "(none)");

    // IPv4 typical case
    ipv4_t ipv4_valid = {.isNull = false, .ip = {192, 168, 1, 1}};
    memset(tmp, 0, sizeof(tmp));
    len = str_formatIpv4(&ipv4_valid, tmp, sizeof(tmp));
    assert_int_equal(len, strlen("192.168.1.1"));
    assert_string_equal(tmp, "192.168.1.1");

    // IPv4 all zeros
    ipv4_t ipv4_zeros = {.isNull = false, .ip = {0, 0, 0, 0}};
    memset(tmp, 0, sizeof(tmp));
    len = str_formatIpv4(&ipv4_zeros, tmp, sizeof(tmp));
    assert_int_equal(len, strlen("0.0.0.0"));
    assert_string_equal(tmp, "0.0.0.0");

    // IPv4 max values
    ipv4_t ipv4_max = {.isNull = false, .ip = {255, 255, 255, 255}};
    memset(tmp, 0, sizeof(tmp));
    len = str_formatIpv4(&ipv4_max, tmp, sizeof(tmp));
    assert_int_equal(len, strlen("255.255.255.255"));
    assert_string_equal(tmp, "255.255.255.255");
}

// Test IPv6 address formatting
static void test_format_ipv6(void **state) {
    (void) state;

    char tmp[IPV6_STR_SIZE_MAX + 1] = {0};

    // IPv6 null case
    ipv6_t ipv6_null = {.isNull = true, .ip = {0}};
    size_t len = str_formatIpv6(&ipv6_null, tmp, sizeof(tmp));
    assert_int_equal(len, strlen("(none)"));
    assert_string_equal(tmp, "(none)");

    // IPv6 all zeros
    ipv6_t ipv6_zeros = {.isNull = false, .ip = {0}};
    memset(tmp, 0, sizeof(tmp));
    len = str_formatIpv6(&ipv6_zeros, tmp, sizeof(tmp));
    assert_int_equal(len, strlen("::"));
    assert_string_equal(tmp, "::");

    // IPv6 loopback
    ipv6_t ipv6_loopback = {.isNull = false, .ip = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}};
    memset(tmp, 0, sizeof(tmp));
    len = str_formatIpv6(&ipv6_loopback, tmp, sizeof(tmp));
    assert_int_equal(len, strlen("::1"));
    assert_string_equal(tmp, "::1");

    // IPv6 typical case
    ipv6_t ipv6_valid = {.isNull = false, .ip = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}};
    memset(tmp, 0, sizeof(tmp));
    len = str_formatIpv6(&ipv6_valid, tmp, sizeof(tmp));
    assert_int_equal(len, strlen("2001:db8::1"));
    assert_string_equal(tmp, "2001:db8::1");
}

// Test IP port formatting
static void test_format_ip_port(void **state) {
    (void) state;

    char tmp[20] = {0};

    // Port null case
    ipport_t port_null = {.isNull = true, .number = 0};
    size_t len = str_formatIpPort(&port_null, tmp, sizeof(tmp));
    assert_int_equal(len, strlen("(none)"));
    assert_string_equal(tmp, "(none)");

    // Port 0
    ipport_t port_zero = {.isNull = false, .number = 0};
    memset(tmp, 0, sizeof(tmp));
    len = str_formatIpPort(&port_zero, tmp, sizeof(tmp));
    assert_int_equal(len, strlen("0"));
    assert_string_equal(tmp, "0");

    // Port typical case
    ipport_t port_http = {.isNull = false, .number = 80};
    memset(tmp, 0, sizeof(tmp));
    len = str_formatIpPort(&port_http, tmp, sizeof(tmp));
    assert_int_equal(len, strlen("80"));
    assert_string_equal(tmp, "80");

    // Port HTTPS
    ipport_t port_https = {.isNull = false, .number = 443};
    memset(tmp, 0, sizeof(tmp));
    len = str_formatIpPort(&port_https, tmp, sizeof(tmp));
    assert_int_equal(len, strlen("443"));
    assert_string_equal(tmp, "443");

    // Port max value
    ipport_t port_max = {.isNull = false, .number = 65535};
    memset(tmp, 0, sizeof(tmp));
    len = str_formatIpPort(&port_max, tmp, sizeof(tmp));
    assert_int_equal(len, strlen("65535"));
    assert_string_equal(tmp, "65535");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_abs_int64),
        cmocka_unit_test(test_format_decimal_basic),
        cmocka_unit_test(test_format_ada_basic),
        cmocka_unit_test(test_format_validity_boundary_mainnet),
        cmocka_unit_test(test_format_validity_boundary),
        cmocka_unit_test(test_format_uint64),
        cmocka_unit_test(test_format_int64),
        cmocka_unit_test(test_is_printable_ascii),
        cmocka_unit_test(test_is_unambiguous_ascii),
        cmocka_unit_test(test_format_ipv4),
        cmocka_unit_test(test_format_ipv6),
        cmocka_unit_test(test_format_ip_port),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
