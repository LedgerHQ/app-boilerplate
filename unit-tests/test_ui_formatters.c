#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <limits.h>

#include <cmocka.h>

#include "ui/ui_formatters.h"
#include "utils/ipUtils.h"
#include "hexUtils.h"
#include "cardano_constants.h"
static void test_format_hex_bytes(void **state) {
    (void) state;

    const uint8_t input[] = {0x00, 0xab, 0xcd};
    char out[10] = {0};

    bool success = format_hex_bytes(input, sizeof(input), out, sizeof(out));
    assert_true(success);
    assert_string_equal(out, "00abcd");
}

static void test_format_uint64(void **state) {
    (void) state;

    struct {
        uint64_t number;
        const char *expected;
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
        bool success = format_uint64(testVectors[i].number, tmp, sizeof(tmp));
        assert_true(success);
        assert_string_equal(tmp, testVectors[i].expected);
    }
}

static void test_format_decimal_amount(void **state) {
    (void) state;

    struct {
        uint64_t amount;
        size_t places;
        const char *expected;
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
        bool success = format_decimal_amount(
            testVectors[i].amount,
            testVectors[i].places,
            tmp,
            sizeof(tmp)
        );
        assert_true(success);
        size_t len = strlen(tmp);
        assert_int_equal(len, strlen(testVectors[i].expected));
        assert_string_equal(tmp, testVectors[i].expected);
    }
}

static void test_format_ada_amount(void **state) {
    (void) state;

    struct {
        uint64_t amount;
        const char *expected;
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
        bool success = format_ada_amount(testVectors[i].amount, tmp, sizeof(tmp));
        assert_true(success);
        size_t len = strlen(tmp);
        assert_int_equal(len, strlen(testVectors[i].expected));
        assert_string_equal(tmp, testVectors[i].expected);
    }
}

static void test_format_validity_boundary(void **state) {
    (void) state;

    // Mainnet uses epoch/slot formatting
    {
        char tmp[100] = {0};
        bool success = format_validity_boundary(
            4492800,
            MAINNET_NETWORK_ID,
            MAINNET_PROTOCOL_MAGIC,
            tmp,
            sizeof(tmp)
        );
        assert_true(success);
        assert_string_equal(tmp, "epoch 208 / slot 0");
    }

    // Testnet uses raw slot formatting
    {
        char tmp[100] = {0};
        bool success = format_validity_boundary(
            12345,
            TESTNET_NETWORK_ID,
            TESTNET_PROTOCOL_MAGIC_LEGACY,
            tmp,
            sizeof(tmp)
        );
        assert_true(success);
        assert_string_equal(tmp, "12345");
    }

    // Mainnet with wrong protocol magic uses raw slot formatting
    {
        char tmp[100] = {0};
        bool success = format_validity_boundary(
            12345,
            MAINNET_NETWORK_ID,
            TESTNET_PROTOCOL_MAGIC_LEGACY,
            tmp,
            sizeof(tmp)
        );
        assert_true(success);
        assert_string_equal(tmp, "12345");
    }

    // Mainnet boundary with large slots (epoch over limit)
    {
        char tmp[100] = {0};
        bool success = format_validity_boundary(
            1000001llu * 432000 + 124,
            MAINNET_NETWORK_ID,
            MAINNET_PROTOCOL_MAGIC,
            tmp,
            sizeof(tmp)
        );
        assert_true(success);
        assert_string_equal(tmp, "epoch more than 1000000");
    }
}

static void test_format_pool_margin(void **state) {
    (void) state;

    struct {
        uint64_t numerator;
        uint64_t denominator;
        const char *expected;
    } testVectors[] = {
        {500, 10000, "5.0 %"},
        {123, 10000, "1.23 %"},
        {1, 3, "33.33 %"},
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        char tmp[20] = {0};
        bool success = format_pool_margin(testVectors[i].numerator,
                                          testVectors[i].denominator,
                                          tmp,
                                          sizeof(tmp));
        assert_true(success);
        assert_string_equal(tmp, testVectors[i].expected);
    }
}

static void test_format_uint16(void **state) {
    (void) state;

    struct {
        uint16_t value;
        const char *expected;
    } testVectors[] = {
        {0, "0"},
        {65535, "65535"},
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        char tmp[20] = {0};
        bool success = format_uint16(testVectors[i].value, tmp, sizeof(tmp));
        assert_true(success);
        assert_string_equal(tmp, testVectors[i].expected);
    }
}

static void test_format_index_with_prefix(void **state) {
    (void) state;

    char tmp[20] = {0};
    bool success = format_index_with_prefix(0, tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "#0");

    memset(tmp, 0, sizeof(tmp));
    success = format_index_with_prefix(42, tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "#42");
}

static void test_format_ipv4(void **state) {
    (void) state;

    char tmp[MAX_IPV4_STR_LENGTH + 2] = {0};  // +2 to check we don't exceed buffer

    ipv4_t ipv4_null = {.isNull = true, .ip = NULL};
    bool success = format_ipv4(&ipv4_null, tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "(none)");

    const uint8_t ipv4_valid_bytes[IPV4_LENGTH] = {192, 168, 1, 1};
    ipv4_t ipv4_valid = {.isNull = false, .ip = ipv4_valid_bytes};
    memset(tmp, 0, sizeof(tmp));
    success = format_ipv4(&ipv4_valid, tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "192.168.1.1");

    const uint8_t ipv4_zeros_bytes[IPV4_LENGTH] = {0, 0, 0, 0};
    ipv4_t ipv4_zeros = {.isNull = false, .ip = ipv4_zeros_bytes};
    memset(tmp, 0, sizeof(tmp));
    success = format_ipv4(&ipv4_zeros, tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "0.0.0.0");

    const uint8_t ipv4_max_bytes[IPV4_LENGTH] = {255, 255, 255, 255};
    ipv4_t ipv4_max = {.isNull = false, .ip = ipv4_max_bytes};
    memset(tmp, 0, sizeof(tmp));
    success = format_ipv4(&ipv4_max, tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "255.255.255.255");
}

static void test_format_ipv6(void **state) {
    (void) state;

    char tmp[MAX_IPV6_STR_LENGTH + 2] = {0};  // +2 to check we don't exceed buffer

    ipv6_t ipv6_null = {.isNull = true, .ip = NULL};
    bool success = format_ipv6(&ipv6_null, tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "(none)");

    const uint8_t ipv6_zeros_bytes[IPV6_LENGTH] = {0};
    ipv6_t ipv6_zeros = {.isNull = false, .ip = ipv6_zeros_bytes};
    memset(tmp, 0, sizeof(tmp));
    success = format_ipv6(&ipv6_zeros, tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "::");

    const uint8_t ipv6_loopback_bytes[IPV6_LENGTH] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    ipv6_t ipv6_loopback = {.isNull = false, .ip = ipv6_loopback_bytes};
    memset(tmp, 0, sizeof(tmp));
    success = format_ipv6(&ipv6_loopback, tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "::1");

    const uint8_t ipv6_valid_bytes[IPV6_LENGTH] = {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    ipv6_t ipv6_valid = {.isNull = false, .ip = ipv6_valid_bytes};
    memset(tmp, 0, sizeof(tmp));
    success = format_ipv6(&ipv6_valid, tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "2001:db8::1");
}

static void test_format_vote_option(void **state) {
    (void) state;

    char tmp[20] = {0};
    bool success = format_vote_option(VOTE_NO, tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "No");

    memset(tmp, 0, sizeof(tmp));
    success = format_vote_option(VOTE_YES, tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "Yes");

    memset(tmp, 0, sizeof(tmp));
    success = format_vote_option(VOTE_ABSTAIN, tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "Abstain");

    memset(tmp, 0, sizeof(tmp));
    success = format_vote_option((vote_t) 99, tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "Unknown");
}

static void test_format_constant_drep(void **state) {
    (void) state;

    char tmp[40] = {0};
    bool success = format_constant_drep(EXT_DREP_ABSTAIN, tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "Abstain");

    memset(tmp, 0, sizeof(tmp));
    success = format_constant_drep(EXT_DREP_NO_CONFIDENCE, tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "No Confidence");
}

static void test_format_certificate_type(void **state) {
    (void) state;

    char tmp[40] = {0};
    bool success = format_certificate_type(CERTIFICATE_STAKE_REGISTRATION, tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "Stake Registration");

    memset(tmp, 0, sizeof(tmp));
    success = format_certificate_type(CERTIFICATE_STAKE_POOL_REGISTRATION, tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "Pool Registration");

    memset(tmp, 0, sizeof(tmp));
    success = format_certificate_type(CERTIFICATE_DREP_UPDATE, tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "DRep Update");
}

static void test_format_url(void **state) {
    (void) state;

    const uint8_t url[] = "example.com";
    char tmp[32] = {0};
    bool success = format_url(url, strlen((const char *) url), tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "example.com");

    char too_small[5] = {0};
    success = format_url(url, strlen((const char *) url), too_small, sizeof(too_small));
    assert_false(success);
}

static void test_format_dns_name(void **state) {
    (void) state;

    const uint8_t dns_name[] = "relay.example.com";
    char tmp[32] = {0};
    bool success = format_dns_name(dns_name, strlen((const char *) dns_name), tmp, sizeof(tmp));
    assert_true(success);
    assert_string_equal(tmp, "relay.example.com");
}

static void test_format_asset_fingerprint_bech32(void **state) {
    (void) state;

    struct {
        const char *policyIdHex;
        const char *assetNameHex;
        const char *expectedBech32;
    } testVectors[] = {
        // Test vectors from CIP 14 proposal
        {"7eae28af2208be856f7a119668ae52a49b73725e326dc16579dcc373",
         "",
         "asset1rjklcrnsdzqp65wjgrg55sy9723kw09mlgvlc3"},

        {"1e349c9bdea19fd6c147626a5260bc44b71635f398b67c59881df209",
         "7eae28af2208be856f7a119668ae52a49b73725e326dc16579dcc373",
         "asset1aqrdypg669jgazruv5ah07nuyqe0wxjhe2el6f"},

        {"1e349c9bdea19fd6c147626a5260bc44b71635f398b67c59881df209",
         "504154415445",
         "asset1hv4p5tv2a837mzqrst04d0dcptdjmluqvdx9k3"},

        {"7eae28af2208be856f7a119668ae52a49b73725e326dc16579dcc373",
         "0000000000000000000000000000000000000000000000000000000000000000",
         "asset1pkpwyknlvul7az0xx8czhl60pyel45rpje4z8w"},
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        uint8_t policyId[MINTING_POLICY_ID_LENGTH] = {0};
        size_t policyIdSize = 0;
        bool success_policy = decode_hex(testVectors[i].policyIdHex, policyId, sizeof(policyId), &policyIdSize);
        assert_true(success_policy);
        assert_int_equal(policyIdSize, MINTING_POLICY_ID_LENGTH);

        uint8_t assetName[MAX_ASSET_NAME_LENGTH] = {0};
        size_t assetNameSize = 0;
        bool success = decode_hex(testVectors[i].assetNameHex, assetName, sizeof(assetName), &assetNameSize);
        assert_true(success);

        char fingerprint[200] = {0};
        success = format_asset_fingerprint_bech32(policyId,
                                                  assetName,
                                                  assetNameSize,
                                                  fingerprint,
                                                  sizeof(fingerprint));
        assert_true(success);
        assert_string_equal(fingerprint, testVectors[i].expectedBech32);
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_format_hex_bytes),
        cmocka_unit_test(test_format_uint64),
        cmocka_unit_test(test_format_decimal_amount),
        cmocka_unit_test(test_format_ada_amount),
        cmocka_unit_test(test_format_validity_boundary),
        cmocka_unit_test(test_format_pool_margin),
        cmocka_unit_test(test_format_uint16),
        cmocka_unit_test(test_format_index_with_prefix),
        cmocka_unit_test(test_format_ipv4),
        cmocka_unit_test(test_format_ipv6),
        cmocka_unit_test(test_format_vote_option),
        cmocka_unit_test(test_format_constant_drep),
        cmocka_unit_test(test_format_certificate_type),
        cmocka_unit_test(test_format_url),
        cmocka_unit_test(test_format_dns_name),
        cmocka_unit_test(test_format_asset_fingerprint_bech32),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
