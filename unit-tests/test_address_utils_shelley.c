#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <cmocka.h>

#include "addressUtils/addressUtilsShelley.h"
#include "addressUtils/addressUtilsByron.h"
#include "addressUtils/bip44.h"
#include "hexUtils.h"

#define HD HARDENED_BIP32
#define MAX_ADDRESS_LENGTH 128

static void init_path(bip44_path_t* path, const uint32_t* elements, size_t len) {
    path->length = len;
    for (size_t i = 0; i < len; i++) {
        path->path[i] = elements[i];
    }
}

static void testcase_derive_address_shelley(address_type_t type,
                                            uint32_t networkIdOrProtocolMagic,
                                            const uint32_t* paymentPath,
                                            size_t paymentPathLen,
                                            staking_data_source_t stakingDataSource,
                                            const uint32_t* stakingPath,
                                            size_t stakingPathLen,
                                            const char* stakingKeyHashHex,
                                            const blockchainPointer_t* pointer,
                                            const char* expectedHex) {
    addressParams_t params = {0};

    if (type == BYRON) {
        params.type = type;
        params.protocolMagic = networkIdOrProtocolMagic;
    } else {
        params.type = type;
        params.networkId = (uint8_t) networkIdOrProtocolMagic;
    }

    params.stakingDataSource = stakingDataSource;

    init_path(&params.paymentKeyPath, paymentPath, paymentPathLen);
    if (stakingPathLen > 0) {
        init_path(&params.stakingKeyPath, stakingPath, stakingPathLen);
    }

    if (stakingKeyHashHex != NULL) {
        uint8_t stakingKeyHash[ADDRESS_KEY_HASH_LENGTH] = {0};
        size_t decodedLen = 0;
        assert_true(strlen(stakingKeyHashHex) == ADDRESS_KEY_HASH_LENGTH * 2);
        assert_true(decode_hex(stakingKeyHashHex, stakingKeyHash, sizeof(stakingKeyHash), &decodedLen));
        assert_int_equal(decodedLen, sizeof(stakingKeyHash));
        memcpy(params.stakingKeyHash, stakingKeyHash, sizeof(stakingKeyHash));
    }

    if (pointer != NULL) {
        params.stakingKeyBlockchainPointer = *pointer;
    }

    uint8_t out[MAX_ADDRESS_LENGTH];
    size_t outSize = deriveAddress(&params, out, sizeof(out));

    uint8_t expected[MAX_ADDRESS_LENGTH] = {0};
    size_t expectedSize = 0;
    assert_true(decode_hex(expectedHex, expected, sizeof(expected), &expectedSize));

    assert_int_equal(outSize, expectedSize);
    assert_memory_equal(out, expected, expectedSize);
}

static void test_address_derivation(void **state) {
    (void) state;

    testcase_derive_address_shelley(
        BYRON,
        MAINNET_PROTOCOL_MAGIC,
        (uint32_t[]){HD + 44, HD + 1815, HD + 0, 1, 55},
        5,
        NO_STAKING,
        NULL,
        0,
        NULL,
        NULL,
        "82d818582183581ca39fa49038d760e5ebfdafe4e6fb28bd5506af6be6687e2278e8f13ba0001a5bce789b");

    testcase_derive_address_shelley(
        BASE_PAYMENT_KEY_STAKE_KEY,
        0x03,
        (uint32_t[]){HD + 1852, HD + 1815, HD + 0, 0, 1},
        5,
        STAKING_KEY_PATH,
        (uint32_t[]){HD + 1852, HD + 1815, HD + 0, 2, 0},
        5,
        NULL,
        NULL,
        "035a53103829a7382c2ab76111fb69f13e69d616824c62058e44f1a8b31d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c");

    testcase_derive_address_shelley(
        BASE_PAYMENT_KEY_STAKE_KEY,
        0x00,
        (uint32_t[]){HD + 1852, HD + 1815, HD + 0, 0, 1},
        5,
        STAKING_KEY_PATH,
        (uint32_t[]){HD + 1852, HD + 1815, HD + 0, 2, 0},
        5,
        NULL,
        NULL,
        "005a53103829a7382c2ab76111fb69f13e69d616824c62058e44f1a8b31d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c");

    testcase_derive_address_shelley(
        BASE_PAYMENT_KEY_STAKE_KEY,
        0x00,
        (uint32_t[]){HD + 1852, HD + 1815, HD + 0, 0, 1},
        5,
        STAKING_KEY_HASH,
        NULL,
        0,
        "1d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c",
        NULL,
        "005a53103829a7382c2ab76111fb69f13e69d616824c62058e44f1a8b31d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c");

    testcase_derive_address_shelley(
        BASE_PAYMENT_KEY_STAKE_KEY,
        0x03,
        (uint32_t[]){HD + 1852, HD + 1815, HD + 0, 0, 1},
        5,
        STAKING_KEY_HASH,
        NULL,
        0,
        "122a946b9ad3d2ddf029d3a828f0468aece76895f15c9efbd69b4277",
        NULL,
        "035a53103829a7382c2ab76111fb69f13e69d616824c62058e44f1a8b3122a946b9ad3d2ddf029d3a828f0468aece76895f15c9efbd69b4277");

    testcase_derive_address_shelley(
        ENTERPRISE_KEY,
        0x00,
        (uint32_t[]){HD + 1852, HD + 1815, HD + 0, 0, 1},
        5,
        NO_STAKING,
        NULL,
        0,
        NULL,
        NULL,
        "605a53103829a7382c2ab76111fb69f13e69d616824c62058e44f1a8b3");

    testcase_derive_address_shelley(
        ENTERPRISE_KEY,
        0x03,
        (uint32_t[]){HD + 1852, HD + 1815, HD + 0, 0, 1},
        5,
        NO_STAKING,
        NULL,
        0,
        NULL,
        NULL,
        "635a53103829a7382c2ab76111fb69f13e69d616824c62058e44f1a8b3");

    testcase_derive_address_shelley(
        POINTER_KEY,
        0x00,
        (uint32_t[]){HD + 1852, HD + 1815, HD + 0, 0, 1},
        5,
        BLOCKCHAIN_POINTER,
        NULL,
        0,
        NULL,
        &(blockchainPointer_t){.blockIndex = 1, .txIndex = 2, .certificateIndex = 3},
        "405a53103829a7382c2ab76111fb69f13e69d616824c62058e44f1a8b3010203");

    testcase_derive_address_shelley(
        POINTER_KEY,
        0x03,
        (uint32_t[]){HD + 1852, HD + 1815, HD + 0, 0, 1},
        5,
        BLOCKCHAIN_POINTER,
        NULL,
        0,
        NULL,
        &(blockchainPointer_t){.blockIndex = 24157, .txIndex = 177, .certificateIndex = 42},
        "435a53103829a7382c2ab76111fb69f13e69d616824c62058e44f1a8b381bc5d81312a");

    testcase_derive_address_shelley(
        POINTER_KEY,
        0x03,
        (uint32_t[]){HD + 1852, HD + 1815, HD + 0, 0, 1},
        5,
        BLOCKCHAIN_POINTER,
        NULL,
        0,
        NULL,
        &(blockchainPointer_t){.blockIndex = 0, .txIndex = 0, .certificateIndex = 0},
        "435a53103829a7382c2ab76111fb69f13e69d616824c62058e44f1a8b3000000");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_address_derivation),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
