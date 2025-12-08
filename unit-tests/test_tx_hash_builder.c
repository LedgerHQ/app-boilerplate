#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <setjmp.h>
#include <stdarg.h>

#include <cmocka.h>

#include "txHashBuilder/txHashBuilder.h"
#include "hexUtils.h"
#include "utils/utils.h"

static size_t decode_hex_buffer(const char* hex, uint8_t* dst, size_t dstSize) {
    size_t decodedLen = 0;
    assert_true(decode_hex(hex, dst, dstSize, &decodedLen));
    return decodedLen;
}

static void test_tx_hash_builder_full(void** state) {
    (void)state;

    tx_hash_builder_t builder = {0};

    txHashBuilder_init(&builder,
                       true,
                       1,
                       1,
                       true,
                       1,
                       1,
                       true,
                       true,
                       true,
                       true,
                       1,
                       1,
                       true,
                       true,
                       true,
                       1,
                       1,
                       true,
                       true);

    tx_input_t input = {0};
    input.index = 0;
    static const char* inputHashHex = "34BBDF0A10E7290AD22E3EE791B6B3C35C206AB8B51BB749A2B06489CEEBF5F4";
    decode_hex_buffer(inputHashHex, input.txHashBuffer, SIZEOF(input.txHashBuffer));

    txHashBuilder_enterInputs(&builder);
    txHashBuilder_addInput(&builder, &input);

    uint8_t mainAddress[200] = {0};
    size_t mainAddressLen = decode_hex_buffer(
        "83581C5F5BEE73ED41FF6C8490DFDB4732178E0216CCF7BADBE1E77D5D7FF8A1"
        "01581E581C1E9A0361BDC37DB7AB7EA2A3F187761877F3DB11211FC7436131F15E00",
        mainAddress,
        sizeof(mainAddress));

    tx_output_destination_t mainDestination = {
        .type = DESTINATION_THIRD_PARTY,
        .address = { .buffer = mainAddress, .size = mainAddressLen },
    };

    tx_output_description_t output = {
        .format = MAP_BABBAGE,
        .destination = mainDestination,
        .amount = 1234567ULL,
        .numAssetGroups = 1,
        .includeDatum = true,
        .includeRefScript = false,
    };

    txHashBuilder_enterOutputs(&builder);
    txHashBuilder_addOutput_topLevelData(&builder, &output);

    static const char* policyIdHex = "0A0B0C0D0E0F101112131415161718191A1B1C1D1E1F2021222324";
    uint8_t policyId[MINTING_POLICY_ID_SIZE] = {0};
    decode_hex_buffer(policyIdHex, policyId, SIZEOF(policyId));
    txHashBuilder_addOutput_tokenGroup(&builder, policyId, SIZEOF(policyId), 1);

    const uint8_t assetName[] = "Token";
    txHashBuilder_addOutput_token(&builder, assetName, sizeof(assetName) - 1, 10);

    static const char* datumHex = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    uint8_t datumHash[OUTPUT_DATUM_HASH_LENGTH] = {0};
    decode_hex_buffer(datumHex, datumHash, SIZEOF(datumHash));
    txHashBuilder_addOutput_datum(&builder, DATUM_HASH, datumHash, SIZEOF(datumHash));

    txHashBuilder_addFee(&builder, 200000ULL);
    txHashBuilder_addTtl(&builder, 126);

    txHashBuilder_enterCertificates(&builder);
    credential_t stakeCredential = {0};
    stakeCredential.type = CREDENTIAL_KEY_HASH;
    static const char* stakeKeyHex = "0C0D0E0F101112131415161718191A1B1C1D1E1F20212223242526";
    decode_hex_buffer(stakeKeyHex, stakeCredential.keyHash, SIZEOF(stakeCredential.keyHash));

    uint8_t poolKeyHash[POOL_KEY_HASH_LENGTH] = {0};
    static const char* poolKeyHashHex = "1C1D1E1F202122232425262728292A2B2C2D2E2F30313233343536";
    decode_hex_buffer(poolKeyHashHex, poolKeyHash, SIZEOF(poolKeyHash));

    txHashBuilder_addCertificate_stakeDelegation(&builder, &stakeCredential, poolKeyHash, SIZEOF(poolKeyHash));

    txHashBuilder_enterWithdrawals(&builder);
    uint8_t rewardAddress[REWARD_ACCOUNT_SIZE] = {0};
    static const char* rewardAddrHex = "E1A1A2A3A4A5A6A7A8A9AAABACADAEB0B1B2B3B4B5B6B7B8B9";
    decode_hex_buffer(rewardAddrHex, rewardAddress, SIZEOF(rewardAddress));
    txHashBuilder_addWithdrawal(&builder, rewardAddress, SIZEOF(rewardAddress), 500000ULL);

    uint8_t auxDataHash[AUX_DATA_HASH_LENGTH] = {0};
    static const char* auxHashHex = "C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3C3";
    decode_hex_buffer(auxHashHex, auxDataHash, SIZEOF(auxDataHash));
    txHashBuilder_addAuxData(&builder, auxDataHash, SIZEOF(auxDataHash));

    txHashBuilder_addValidityIntervalStart(&builder, 8000ULL);

    txHashBuilder_enterMint(&builder);
    txHashBuilder_addMint_topLevelData(&builder, 1);
    txHashBuilder_addMint_tokenGroup(&builder, policyId, SIZEOF(policyId), 1);
    txHashBuilder_addMint_token(&builder, assetName, sizeof(assetName) - 1, 5);

    uint8_t scriptDataHash[SCRIPT_DATA_HASH_LENGTH] = {0};
    static const char* scriptHashHex = "0102030405060708090A0B0C0D0E0F101112131415161718191A1B1C1D1E1F20";
    decode_hex_buffer(scriptHashHex, scriptDataHash, SIZEOF(scriptDataHash));
    txHashBuilder_addScriptDataHash(&builder, scriptDataHash, SIZEOF(scriptDataHash));

    tx_input_t collateralInput = {0};
    collateralInput.index = 1;
    decode_hex_buffer("34BBDF0A10E7290AD22E3EE791B6B3C35C206AB8B51BB749A2B06489CEEBF5F4",
                      collateralInput.txHashBuffer,
                      SIZEOF(collateralInput.txHashBuffer));
    txHashBuilder_enterCollateralInputs(&builder);
    txHashBuilder_addCollateralInput(&builder, &collateralInput);

    uint8_t requiredSigner[ADDRESS_KEY_HASH_LENGTH] = {0};
    static const char* signerHex = "1F202122232425262728292A2B2C2D2E2F303132333435363738";
    decode_hex_buffer(signerHex, requiredSigner, SIZEOF(requiredSigner));

    txHashBuilder_enterRequiredSigners(&builder);
    txHashBuilder_addRequiredSigner(&builder, requiredSigner, SIZEOF(requiredSigner));

    txHashBuilder_addNetworkId(&builder, 1);

    uint8_t collateralAddress[200] = {0};
    size_t collateralAddressLen = decode_hex_buffer(
        "83581CADA4052647C47745ABFC9E04D7DC5C5C0A85428F5B741BE6687E6005A1"
        "01581E581CD8669B0C1A9F2FCCB28D3EF58EF8EFAD73AEAD7117B6559A5F8578"
        "1300",
        collateralAddress,
        sizeof(collateralAddress));
    tx_output_destination_t collateralDestination = {
        .type = DESTINATION_THIRD_PARTY,
        .address = { .buffer = collateralAddress, .size = collateralAddressLen },
    };
    tx_output_description_t collateralOutput = {
        .format = ARRAY_LEGACY,
        .destination = collateralDestination,
        .amount = 500000ULL,
        .numAssetGroups = 0,
        .includeDatum = false,
        .includeRefScript = false,
    };
    txHashBuilder_addCollateralOutput(&builder, &collateralOutput);

    txHashBuilder_addTotalCollateral(&builder, 500000ULL);

    tx_input_t referenceInput = {0};
    referenceInput.index = 2;
    decode_hex_buffer("EA34DF0A10E7290AD22E3EE791B6B3C35C206AB8B51BB749A2B06489CEEBF5F5",
                      referenceInput.txHashBuffer,
                      SIZEOF(referenceInput.txHashBuffer));
    txHashBuilder_enterReferenceInputs(&builder);
    txHashBuilder_addReferenceInput(&builder, &referenceInput);

    voter_t voter = {0};
    voter.type = VOTER_STAKE_POOL_KEY_HASH;
    decode_hex_buffer("2A2B2C2D2E2F303132333435363738393A3B3C3D3E3F40414243",
                      voter.keyHash,
                      SIZEOF(voter.keyHash));

    gov_action_id_t govAction = {0};
    govAction.govActionIndex = 7;
    decode_hex_buffer("45BBDF0A10E7290AD22E3EE791B6B3C35C206AB8B51BB749A2B06489CEEBF5F1",
                      govAction.txHashBuffer,
                      SIZEOF(govAction.txHashBuffer));

    voting_procedure_t votingProcedure = {0};
    votingProcedure.vote = VOTE_YES;
    votingProcedure.anchor.isIncluded = false;

    txHashBuilder_enterVotingProcedures(&builder);
    txHashBuilder_addVotingProcedure(&builder, &voter, &govAction, &votingProcedure);

    txHashBuilder_addTreasury(&builder, 1ULL);
    txHashBuilder_addDonation(&builder, 2ULL);

    uint8_t result[TX_HASH_LENGTH] = {0};
    txHashBuilder_finalize(&builder, result, SIZEOF(result));

    static const char* expectedHex = "BFBC406FD11F7381A7084410210596A5A5AFD4F6A490659495FBA3ED564B67C8";
    uint8_t expected[TX_HASH_LENGTH] = {0};
    decode_hex_buffer(expectedHex, expected, SIZEOF(expected));

    assert_memory_equal(result, expected, SIZEOF(result));
}

static void test_tx_hash_builder_minimal(void** state) {
    (void)state;

    tx_hash_builder_t builder = {0};
    txHashBuilder_init(&builder,
                       false,
                       1,
                       5,
                       false,
                       0,
                       0,
                       false,
                       false,
                       false,
                       false,
                       0,
                       0,
                       false,
                       false,
                       false,
                       0,
                       0,
                       false,
                       false);

    tx_input_t input = {0};
    input.index = 0;
    static const char* inputHashHex = "34BBDF0A10E7290AD22E3EE791B6B3C35C206AB8B51BB749A2B06489CEEBF5F4";
    decode_hex_buffer(inputHashHex, input.txHashBuffer, SIZEOF(input.txHashBuffer));

    txHashBuilder_enterInputs(&builder);
    txHashBuilder_addInput(&builder, &input);

    static const struct {
        const char* address;
        uint64_t amount;
    } outputs[] = {
        {
            "83581C5F5BEE73ED41FF6C8490DFDB4732178E0216CCF7BADBE1E77D5D7FF8A1"
            "01581E581C1E9A0361BDC37DB7AB7EA2A3F187761877F3DB11211FC7436131F15E00",
            1673668090925ULL,
        },
        {
            "83581CADA4052647C47745ABFC9E04D7DC5C5C0A85428F5B741BE6687E6005A1"
            "01581E581CD8669B0C1A9F2FCCB28D3EF58EF8EFAD73AEAD7117B6559A5F85781"
            "300",
            372500000ULL,
        },
        {
            "83581C6532CAADC0B498BE1813D12F33BF81D68D5662255CC640B881A29315A1"
            "01581E581CCA3E553C9C63C580936DF7433AAC461E4EFB6CE966206E083AF22D"
            "0E00",
            433000500ULL,
        },
        {
            "83581C6FD85CFE0AE8C346552717424229D5AC928E72B0CBD5587A5D9BD8E5A1"
            "01581E581C2B0B011BA3683D2EB420332A084FE7ECBDEFA204C415CD7AA17E21"
            "6D00",
            3280715000ULL,
        },
        {
            "83581C431923E34D95851FBA3C88E99D9D366EB1D595E5436C68DA1B4699A5A1"
            "01581E581C3054E511BD5ACD29E7540B417600367915AFA6F95B1A40246AA4FC"
            "9F00",
            2035261700ULL,
        },
    };

    txHashBuilder_enterOutputs(&builder);
    for (size_t i = 0; i < sizeof(outputs) / sizeof(outputs[0]); i++) {
        uint8_t addressBuf[200] = {0};
        size_t addressLen = decode_hex_buffer(outputs[i].address, addressBuf, sizeof(addressBuf));

        tx_output_destination_t dest = {
            .type = DESTINATION_THIRD_PARTY,
            .address = { .buffer = addressBuf, .size = addressLen },
        };
        tx_output_description_t output = {
            .format = MAP_BABBAGE,
            .destination = dest,
            .amount = outputs[i].amount,
            .numAssetGroups = 0,
            .includeDatum = false,
            .includeRefScript = false,
        };

        txHashBuilder_addOutput_topLevelData(&builder, &output);
    }

    txHashBuilder_addFee(&builder, 0ULL);

    uint8_t result[TX_HASH_LENGTH] = {0};
    txHashBuilder_finalize(&builder, result, SIZEOF(result));

    static const char* expectedHex = "E831231470909FB213520983E1D478388E4093D81D507041B20A95406F44E2D0";
    uint8_t expected[TX_HASH_LENGTH] = {0};
    decode_hex_buffer(expectedHex, expected, SIZEOF(expected));

    assert_memory_equal(result, expected, SIZEOF(result));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_tx_hash_builder_minimal),
        cmocka_unit_test(test_tx_hash_builder_full),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
