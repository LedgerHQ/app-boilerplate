#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <cmocka.h>

#include "cardano_constants.h"
#include "addressUtils/addressUtilsByron.h"
#include "hexUtils.h"

// Test case for successful protocol magic extraction
static void testcase_extractProtocolMagicSucceeds(const char* addressHex, uint32_t expectedProtocolMagic) {
    uint8_t address[100] = {0};
    size_t addressSize;
    bool success = decode_hex(addressHex, address, sizeof(address), &addressSize);
    assert_true(success);

    uint32_t protocolMagic = 0;
    bool extractSuccess = extractProtocolMagic(address, addressSize, &protocolMagic);

    assert_true(extractSuccess);
    assert_int_equal(protocolMagic, expectedProtocolMagic);
}

// Test case for failed protocol magic extraction
static void testcase_extractProtocolMagicFails(const char* addressHex) {
    uint8_t address[100] = {0};
    size_t addressSize;
    bool success = decode_hex(addressHex, address, sizeof(address), &addressSize);
    assert_true(success);

    uint32_t protocolMagic = 0;
    bool extractSuccess = extractProtocolMagic(address, addressSize, &protocolMagic);

    assert_false(extractSuccess);
}

// ======================== Protocol Magic Extraction Tests ========================

static void test_extract_protocol_magic_mainnet_simple(void **state) {
    (void) state;

    testcase_extractProtocolMagicSucceeds(
        "82d818582183581cb1999ee43d0c3a9fe4a1a5d959ae87069781fbb7f60ff7e8e0136881a0001ad7ed912f",
        MAINNET_PROTOCOL_MAGIC);
}

static void test_extract_protocol_magic_mainnet_complex1(void **state) {
    (void) state;

    testcase_extractProtocolMagicSucceeds(
        "82d818584283581cd2348b8ef7b8a6d1c922efa499c669b151eeef99e4ce3521e88223f8a101581e581cf281e6"
        "48a89015a9861bd9e992414d1145ddaf80690be53235b0e2e5001a19983465",
        MAINNET_PROTOCOL_MAGIC);
}

static void test_extract_protocol_magic_mainnet_complex2(void **state) {
    (void) state;

    testcase_extractProtocolMagicSucceeds(
        "82d818584983581c9c708538a763ff27169987a489e35057ef3cd3778c05e96f7ba9450ea201581e581c9c1722"
        "f7e446689256e1a30260f3510d558d99d0c391f2ba89cb697702451a4170cb17001a6979126c",
        TESTNET_PROTOCOL_MAGIC_LEGACY);
}

static void test_extract_protocol_magic_custom_magic(void **state) {
    (void) state;

    testcase_extractProtocolMagicSucceeds(
        "82d818582583581cb1999ee43d0c3a9fe4a1a5d959ae87069781fbb7f60ff7e8e0136881a10242182a001a2b7c"
        "56f6",
        42);
}

// ======================== Protocol Magic Extraction Failure Tests ========================

static void test_extract_protocol_magic_invalid_cbor(void **state) {
    (void) state;

    // Invalid CBOR
    testcase_extractProtocolMagicFails("deadbeef");
}

static void test_extract_protocol_magic_shelley_address(void **state) {
    (void) state;

    // Shelley address (not Byron format)
    testcase_extractProtocolMagicFails(
        "035a53103829a7382c2ab76111fb69f13e69d616824c62058e44f1a8b31d227aefa4b773149170885aadba30aa"
        "b3127cc611ddbc4999def61c");
}

static void test_extract_protocol_magic_explicit_mainnet(void **state) {
    (void) state;

    // Mainnet protocol magic explicitly encoded (invalid structure)
    testcase_extractProtocolMagicFails(
        "82d818582883581ca1eda96a9952a56c983d9f49117f935af325e8a6c9d38496e945faa8a102451a2d964a0900"
        "1a099ade84");
}

static void test_extract_protocol_magic_too_many_attributes(void **state) {
    (void) state;

    // Too many keys in address attributes
    testcase_extractProtocolMagicFails(
        "82d818583183581ca1eda96a9952a56c983d9f49117f935af325e8a6c9d38496e945faa8a40142182f0242182f"
        "0342182f0442182f001a965d526c");
}

static void test_extract_protocol_magic_attribute_not_bytes(void **state) {
    (void) state;

    // Address attributes value not bytes
    testcase_extractProtocolMagicFails(
        "82d818582483581ca1eda96a9952a56c983d9f49117f935af325e8a6c9d38496e945faa8a101182f001a1abe13"
        "ed");
}

static void test_extract_protocol_magic_invalid_crc32(void **state) {
    (void) state;

    // Invalid crc32 checksum
    testcase_extractProtocolMagicFails(
        "82d818582183581cb1999ee43d0c3a9fe4a1a5d959ae87069781fbb7f60ff7e8e0136881a0001ad7ed912e");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        // Protocol magic extraction success tests
        cmocka_unit_test(test_extract_protocol_magic_mainnet_simple),
        cmocka_unit_test(test_extract_protocol_magic_mainnet_complex1),
        cmocka_unit_test(test_extract_protocol_magic_mainnet_complex2),
        cmocka_unit_test(test_extract_protocol_magic_custom_magic),

        // Protocol magic extraction failure tests
        cmocka_unit_test(test_extract_protocol_magic_invalid_cbor),
        cmocka_unit_test(test_extract_protocol_magic_shelley_address),
        cmocka_unit_test(test_extract_protocol_magic_explicit_mainnet),
        cmocka_unit_test(test_extract_protocol_magic_too_many_attributes),
        cmocka_unit_test(test_extract_protocol_magic_attribute_not_bytes),
        cmocka_unit_test(test_extract_protocol_magic_invalid_crc32),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
