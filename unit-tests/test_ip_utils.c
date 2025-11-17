#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <cmocka.h>

#include "utils/ipUtils.h"

// ======================== IPv4 Tests ========================

static void test_ipv4_basic(void **state) {
    (void) state;

    // Test basic IPv4 address: 192.168.1.1
    uint8_t ipv4_data[] = {192, 168, 1, 1};
    char buffer[IPV4_STR_SIZE_MAX + 1];  // +1 to avoid assertion failure

    inet_ntop4(ipv4_data, buffer, sizeof(buffer));

    assert_string_equal(buffer, "192.168.1.1");
}

static void test_ipv4_zeros(void **state) {
    (void) state;

    // Test IPv4 with zeros: 0.0.0.0
    uint8_t ipv4_data[] = {0, 0, 0, 0};
    char buffer[IPV4_STR_SIZE_MAX + 1];  // +1 to avoid assertion failure

    inet_ntop4(ipv4_data, buffer, sizeof(buffer));

    assert_string_equal(buffer, "0.0.0.0");
}

static void test_ipv4_max(void **state) {
    (void) state;

    // Test IPv4 with max values: 255.255.255.255
    uint8_t ipv4_data[] = {255, 255, 255, 255};
    char buffer[IPV4_STR_SIZE_MAX + 1];  // +1 to avoid assertion failure

    inet_ntop4(ipv4_data, buffer, sizeof(buffer));

    assert_string_equal(buffer, "255.255.255.255");
}

static void test_ipv4_loopback(void **state) {
    (void) state;

    // Test IPv4 loopback: 127.0.0.1
    uint8_t ipv4_data[] = {127, 0, 0, 1};
    char buffer[IPV4_STR_SIZE_MAX + 1];  // +1 to avoid assertion failure

    inet_ntop4(ipv4_data, buffer, sizeof(buffer));

    assert_string_equal(buffer, "127.0.0.1");
}

static void test_ipv4_broadcast(void **state) {
    (void) state;

    // Test IPv4 broadcast: 255.255.255.255 (already tested above, but for completeness)
    uint8_t ipv4_data[] = {255, 255, 255, 255};
    char buffer[IPV4_STR_SIZE_MAX + 1];  // +1 to avoid assertion failure

    inet_ntop4(ipv4_data, buffer, sizeof(buffer));

    assert_string_equal(buffer, "255.255.255.255");
}

static void test_ipv4_mixed_values(void **state) {
    (void) state;

    // Test IPv4 with mixed values: 10.20.30.40
    uint8_t ipv4_data[] = {10, 20, 30, 40};
    char buffer[IPV4_STR_SIZE_MAX + 1];  // +1 to avoid assertion failure

    inet_ntop4(ipv4_data, buffer, sizeof(buffer));

    assert_string_equal(buffer, "10.20.30.40");
}

// ======================== IPv6 Tests ========================

static void test_ipv6_link_local(void **state) {
    (void) state;

    // Test IPv6 link-local with zero compression
    // in:  fe80:0000:0000:0000:a299:9bff:fe18:50d1
    // out: fe80::a299:9bff:fe18:50d1
    uint8_t ipv6_data[] = {0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0xa2, 0x99, 0x9b, 0xff, 0xfe, 0x18, 0x50, 0xd1};
    char buffer[IPV6_STR_SIZE_MAX + 1];  // +1 to avoid assertion failure

    inet_ntop6(ipv6_data, buffer, sizeof(buffer));

    assert_string_equal(buffer, "fe80::a299:9bff:fe18:50d1");
}

static void test_ipv6_documentation_prefix(void **state) {
    (void) state;

    // Test IPv6 documentation prefix with zero compression
    // in:  2001:0db8:1111:000a:00b0:0000:0000:0200
    // out: 2001:db8:1111:a:b0::200
    uint8_t ipv6_data[] = {0x20, 0x01, 0x0d, 0xb8, 0x11, 0x11, 0x00, 0x0a,
                           0x00, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00};
    char buffer[IPV6_STR_SIZE_MAX + 1];  // +1 to avoid assertion failure

    inet_ntop6(ipv6_data, buffer, sizeof(buffer));

    assert_string_equal(buffer, "2001:db8:1111:a:b0::200");
}

static void test_ipv6_ipv4_mapped(void **state) {
    (void) state;

    // Test IPv6 with embedded IPv4 address
    // in:  0:0:0:0:0:ffff:c000:280 (::ffff:192.0.2.128)
    // out: ::ffff:192.0.2.128
    uint8_t ipv6_data[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0xff, 0xff, 0xc0, 0x00, 0x02, 0x80};
    char buffer[IPV6_STR_SIZE_MAX + 1];  // +1 to avoid assertion failure

    inet_ntop6(ipv6_data, buffer, sizeof(buffer));

    assert_string_equal(buffer, "::ffff:192.0.2.128");
}

static void test_ipv6_all_zeros(void **state) {
    (void) state;

    // Test IPv6 all zeros: ::
    uint8_t ipv6_data[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    char buffer[IPV6_STR_SIZE_MAX + 1];  // +1 to avoid assertion failure

    inet_ntop6(ipv6_data, buffer, sizeof(buffer));

    assert_string_equal(buffer, "::");
}

static void test_ipv6_loopback(void **state) {
    (void) state;

    // Test IPv6 loopback: ::1
    uint8_t ipv6_data[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    char buffer[IPV6_STR_SIZE_MAX + 1];  // +1 to avoid assertion failure

    inet_ntop6(ipv6_data, buffer, sizeof(buffer));

    assert_string_equal(buffer, "::1");
}

static void test_ipv6_no_compression(void **state) {
    (void) state;

    // Test IPv6 address with no consecutive zero fields
    // 2001:db8:85a3:8d3:1319:8a2e:370:7348
    uint8_t ipv6_data[] = {0x20, 0x01, 0x0d, 0xb8, 0x85, 0xa3, 0x08, 0xd3,
                           0x13, 0x19, 0x8a, 0x2e, 0x03, 0x70, 0x73, 0x48};
    char buffer[IPV6_STR_SIZE_MAX + 1];  // +1 to avoid assertion failure

    inet_ntop6(ipv6_data, buffer, sizeof(buffer));

    assert_string_equal(buffer, "2001:db8:85a3:8d3:1319:8a2e:370:7348");
}

static void test_ipv6_trailing_zeros(void **state) {
    (void) state;

    // Test IPv6 with trailing zero fields (should compress)
    // 2001:db8:85a3::
    uint8_t ipv6_data[] = {0x20, 0x01, 0x0d, 0xb8, 0x85, 0xa3, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    char buffer[IPV6_STR_SIZE_MAX + 1];  // +1 to avoid assertion failure

    inet_ntop6(ipv6_data, buffer, sizeof(buffer));

    assert_string_equal(buffer, "2001:db8:85a3::");
}

static void test_ipv6_leading_zeros(void **state) {
    (void) state;

    // Test IPv6 with leading zero fields
    // Note: inet_ntop6 implements special handling for IPv4-mapped addresses
    // when there are 6 leading zeros and bytes 10-11 are 0xffff (or 5 zeros + 0xffff)
    // This address (0000... + 1234:5678) doesn't match that pattern,
    // so it gets output as the equivalent IPv4 format (::18.52.86.120)
    uint8_t ipv6_data[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x12, 0x34, 0x56, 0x78};
    char buffer[IPV6_STR_SIZE_MAX + 1];  // +1 to avoid assertion failure

    inet_ntop6(ipv6_data, buffer, sizeof(buffer));

    assert_string_equal(buffer, "::18.52.86.120");
}

static void test_ipv6_middle_zeros(void **state) {
    (void) state;

    // Test IPv6 with middle zero fields
    // 2001:db8::1
    uint8_t ipv6_data[] = {0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    char buffer[IPV6_STR_SIZE_MAX + 1];  // +1 to avoid assertion failure

    inet_ntop6(ipv6_data, buffer, sizeof(buffer));

    assert_string_equal(buffer, "2001:db8::1");
}

static void test_ipv6_multicast(void **state) {
    (void) state;

    // Test IPv6 multicast address
    // ff02::1 (all nodes address)
    uint8_t ipv6_data[] = {0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    char buffer[IPV6_STR_SIZE_MAX + 1];  // +1 to avoid assertion failure

    inet_ntop6(ipv6_data, buffer, sizeof(buffer));

    assert_string_equal(buffer, "ff02::1");
}

static void test_ipv6_ipv4_compatible(void **state) {
    (void) state;

    // Test IPv6 compatible address (deprecated but should still work)
    // Similar to leading_zeros case: 6 zero fields + non-0xffff at bytes 10-11
    // produces IPv4 format output
    uint8_t ipv6_data[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x02, 0x80};
    char buffer[IPV6_STR_SIZE_MAX + 1];  // +1 to avoid assertion failure

    inet_ntop6(ipv6_data, buffer, sizeof(buffer));

    // The function treats this as an IPv4-compatible address and outputs it as IPv4
    assert_string_equal(buffer, "::192.0.2.128");
}

static void test_ipv6_all_ones(void **state) {
    (void) state;

    // Test IPv6 address with all f values
    uint8_t ipv6_data[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                           0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    char buffer[IPV6_STR_SIZE_MAX + 1];  // +1 to avoid assertion failure

    inet_ntop6(ipv6_data, buffer, sizeof(buffer));

    assert_string_equal(buffer, "ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
}

static void test_ipv6_single_zero_field(void **state) {
    (void) state;

    // Test IPv6 with single zero field (should NOT compress - only fields with len >= 2)
    // 2001:db8:0:85a3:8d3:1319:8a2e:370
    uint8_t ipv6_data[] = {0x20, 0x01, 0x0d, 0xb8, 0x00, 0x00, 0x85, 0xa3,
                           0x08, 0xd3, 0x13, 0x19, 0x8a, 0x2e, 0x03, 0x70};
    char buffer[IPV6_STR_SIZE_MAX + 1];  // +1 to avoid assertion failure

    inet_ntop6(ipv6_data, buffer, sizeof(buffer));

    assert_string_equal(buffer, "2001:db8:0:85a3:8d3:1319:8a2e:370");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        // IPv4 tests
        cmocka_unit_test(test_ipv4_basic),
        cmocka_unit_test(test_ipv4_zeros),
        cmocka_unit_test(test_ipv4_max),
        cmocka_unit_test(test_ipv4_loopback),
        cmocka_unit_test(test_ipv4_broadcast),
        cmocka_unit_test(test_ipv4_mixed_values),

        // IPv6 tests from old app
        cmocka_unit_test(test_ipv6_link_local),
        cmocka_unit_test(test_ipv6_documentation_prefix),
        cmocka_unit_test(test_ipv6_ipv4_mapped),

        // Additional IPv6 edge cases
        cmocka_unit_test(test_ipv6_all_zeros),
        cmocka_unit_test(test_ipv6_loopback),
        cmocka_unit_test(test_ipv6_no_compression),
        cmocka_unit_test(test_ipv6_trailing_zeros),
        cmocka_unit_test(test_ipv6_leading_zeros),
        cmocka_unit_test(test_ipv6_middle_zeros),
        cmocka_unit_test(test_ipv6_multicast),
        cmocka_unit_test(test_ipv6_ipv4_compatible),
        cmocka_unit_test(test_ipv6_all_ones),
        cmocka_unit_test(test_ipv6_single_zero_field),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
