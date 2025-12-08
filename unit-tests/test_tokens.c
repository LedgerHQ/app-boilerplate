#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <cmocka.h>

#include "app_tokens/app_tokens.h"
#include "hexUtils.h"

// Test asset fingerprint derivation (CIP-14)
static void test_asset_fingerprint(void **state) {
    (void) state;

    struct {
        const char* policyIdHex;
        const char* assetNameHex;
        const char* expected;
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
        uint8_t policyId[28] = {0};
        size_t policyIdSize;
        bool success_policy = decode_hex(testVectors[i].policyIdHex, policyId, sizeof(policyId), &policyIdSize);
        assert_true(success_policy);

        uint8_t assetName[32] = {0};
        size_t assetNameSize;
        bool success = decode_hex(testVectors[i].assetNameHex, assetName, sizeof(assetName), &assetNameSize);
        assert_true(success);

        char fingerprint[200] = {0};
        deriveAssetFingerprintBech32(policyId,
                                     sizeof(policyId),
                                     assetName,
                                     assetNameSize,
                                     fingerprint,
                                     sizeof(fingerprint));

        assert_string_equal(fingerprint, testVectors[i].expected);
    }
}

// Test token amount formatting for outputs
static void test_format_token_amount_output(void **state) {
    (void) state;

    // Test case 1: Known token with decimal places
    uint8_t policyId1[] = {
        0x94, 0xcb, 0xb4, 0xfc, 0xbc, 0xaa, 0x29, 0x75,
        0x77, 0x9f, 0x27, 0x3b, 0x26, 0x3e, 0xb3, 0xb5,
        0xf2, 0x4a, 0x99, 0x51, 0xe4, 0x46, 0xd6, 0xdc,
        0x4c, 0x13, 0x58, 0x64
    };
    uint8_t assetName1[] = {0x52, 0x45, 0x56, 0x55};  // "REVU"

    token_group_t group1 = {0};
    memcpy(group1.policyId, policyId1, sizeof(policyId1));

    char output1[60] = {0};
    str_formatTokenAmountOutput(&group1, assetName1, sizeof(assetName1), 234, output1, sizeof(output1));
    assert_string_equal(output1, "0.00000234 REVU");

    // Test case 2: Unknown token (no decimal places)
    uint8_t policyId2[] = {
        0xaa, 0xcb, 0xb4, 0xfc, 0xbc, 0xaa, 0x29, 0x75,
        0x77, 0x9f, 0x27, 0x3b, 0x26, 0x3e, 0xb3, 0xb5,
        0xf2, 0x4a, 0x99, 0x51, 0xe4, 0x46, 0xd6, 0xdc,
        0x4c, 0x13, 0x58, 0x64
    };

    token_group_t group2 = {0};
    memcpy(group2.policyId, policyId2, sizeof(policyId2));

    char output2[60] = {0};
    str_formatTokenAmountOutput(&group2, assetName1, sizeof(assetName1), 2345, output2, sizeof(output2));
    assert_string_equal(output2, "2,345 (unknown decimals)");
}

// Test token amount formatting for minting
static void test_format_token_amount_mint(void **state) {
    (void) state;

    // Test case 1: Known token with decimal places
    uint8_t policyId1[] = {
        0x94, 0xcb, 0xb4, 0xfc, 0xbc, 0xaa, 0x29, 0x75,
        0x77, 0x9f, 0x27, 0x3b, 0x26, 0x3e, 0xb3, 0xb5,
        0xf2, 0x4a, 0x99, 0x51, 0xe4, 0x46, 0xd6, 0xdc,
        0x4c, 0x13, 0x58, 0x64
    };
    uint8_t assetName1[] = {0x52, 0x45, 0x56, 0x55};  // "REVU"

    token_group_t group1 = {0};
    memcpy(group1.policyId, policyId1, sizeof(policyId1));

    // Test negative amount (burning)
    char mint1[60] = {0};
    str_formatTokenAmountMint(&group1, assetName1, sizeof(assetName1), -234, mint1, sizeof(mint1));
    assert_string_equal(mint1, "-0.00000234 REVU");

    // Test positive amount (minting)
    char mint2[60] = {0};
    str_formatTokenAmountMint(&group1, assetName1, sizeof(assetName1), 234, mint2, sizeof(mint2));
    assert_string_equal(mint2, " 0.00000234 REVU");

    // Test case 2: Unknown token
    uint8_t policyId2[] = {
        0xaa, 0xcb, 0xb4, 0xfc, 0xbc, 0xaa, 0x29, 0x75,
        0x77, 0x9f, 0x27, 0x3b, 0x26, 0x3e, 0xb3, 0xb5,
        0xf2, 0x4a, 0x99, 0x51, 0xe4, 0x46, 0xd6, 0xdc,
        0x4c, 0x13, 0x58, 0x64
    };

    token_group_t group2 = {0};
    memcpy(group2.policyId, policyId2, sizeof(policyId2));

    char mint3[60] = {0};
    str_formatTokenAmountMint(&group2, assetName1, sizeof(assetName1), 2345, mint3, sizeof(mint3));
    assert_string_equal(mint3, " 2,345 (unknown decimals)");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_asset_fingerprint),
        cmocka_unit_test(test_format_token_amount_output),
        cmocka_unit_test(test_format_token_amount_mint),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
