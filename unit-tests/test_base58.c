#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <cmocka.h>

#include "base58.h"
#include "utils/hexUtils.h"

static void test_base58_empty(void **state) {
    (void) state;

    // Test empty input
    uint8_t inputBuffer[1] = {0};
    char outputStr[100] = {0};
    size_t outputLen = base58_encode(inputBuffer, 0, outputStr, sizeof(outputStr));
    assert_int_equal(outputLen, 0);
}

static void test_base58_single_bytes(void **state) {
    (void) state;

    // Test single byte encodings
    struct {
        const char* inputHex;
        const char* expectedStr;
    } testVectors[] = {
        {"ab", "3x"},
        {"16", "P"},
        {"f2", "5B"},
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        uint8_t inputBuffer[100] = {0};
        size_t inputSize = decode_hex(testVectors[i].inputHex, inputBuffer, sizeof(inputBuffer));

        char outputStr[100] = {0};
        size_t outputLen = base58_encode(inputBuffer, inputSize, outputStr, sizeof(outputStr));

        assert_int_equal(outputLen, strlen(testVectors[i].expectedStr));
        assert_string_equal(outputStr, testVectors[i].expectedStr);
    }
}

static void test_base58_multi_byte(void **state) {
    (void) state;

    // Test multi-byte encodings
    struct {
        const char* inputHex;
        const char* expectedStr;
    } testVectors[] = {
        {"a1b3", "DJi"},
        {"25b6", "3sT"},
        {"ffff", "LUv"},
        {"0000", "11"},
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        uint8_t inputBuffer[100] = {0};
        size_t inputSize = decode_hex(testVectors[i].inputHex, inputBuffer, sizeof(inputBuffer));

        char outputStr[100] = {0};
        size_t outputLen = base58_encode(inputBuffer, inputSize, outputStr, sizeof(outputStr));

        assert_int_equal(outputLen, strlen(testVectors[i].expectedStr));
        assert_string_equal(outputStr, testVectors[i].expectedStr);
    }
}

static void test_base58_leading_zeros(void **state) {
    (void) state;

    // Test that leading zero bytes are preserved as '1' characters
    struct {
        const char* inputHex;
        const char* expectedStr;
    } testVectors[] = {
        {"00000000ab", "11113x"},
        {"00000000df256631", "11116hpoSQ"},
        {"2536000000", "5CVj3Vq"},
        {"0000000000361200000000", "11111TvgAkW5V"},
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        uint8_t inputBuffer[100] = {0};
        size_t inputSize = decode_hex(testVectors[i].inputHex, inputBuffer, sizeof(inputBuffer));

        char outputStr[100] = {0};
        size_t outputLen = base58_encode(inputBuffer, inputSize, outputStr, sizeof(outputStr));

        assert_int_equal(outputLen, strlen(testVectors[i].expectedStr));
        assert_string_equal(outputStr, testVectors[i].expectedStr);
    }
}

static void test_base58_cardano_address(void **state) {
    (void) state;

    // Test Cardano Byron address encoding
    struct {
        const char* inputHex;
        const char* expectedStr;
    } testVectors[] = {
        {"82d818582183581ce63175c654dfd93a9290342a067158dc0f57a1108ddbd8cace3839bda0001a0a0e41ce",
         "Ae2tdPwUPEZKmwoy3AU3cXb5Chnasj6mvVNxV1H11997q3VW5ihbSfQwGpm"},

        {"82d818583983581c07d99d3987090111d70b83e21c1db61acdb659d45cc1b5769a77ae11a1015655c94dbc8f2"
         "a15f95499becfbf9f2de442bce11eacd1001abd57ca7a",
         "4swhHtxKapQbj3TZEipgtp7NQzcRWDYqCxXYoPQWjGyHmhxS1w1TjUEszCQT1sQucGwmPQMYdv1FYs3d51Kgoubvi"
         "PBf"},
    };

    for (size_t i = 0; i < sizeof(testVectors) / sizeof(testVectors[0]); i++) {
        uint8_t inputBuffer[200] = {0};
        size_t inputSize = decode_hex(testVectors[i].inputHex, inputBuffer, sizeof(inputBuffer));

        char outputStr[200] = {0};
        size_t outputLen = base58_encode(inputBuffer, inputSize, outputStr, sizeof(outputStr));

        assert_int_equal(outputLen, strlen(testVectors[i].expectedStr));
        assert_string_equal(outputStr, testVectors[i].expectedStr);
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_base58_empty),
        cmocka_unit_test(test_base58_single_bytes),
        cmocka_unit_test(test_base58_multi_byte),
        cmocka_unit_test(test_base58_leading_zeros),
        cmocka_unit_test(test_base58_cardano_address),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
