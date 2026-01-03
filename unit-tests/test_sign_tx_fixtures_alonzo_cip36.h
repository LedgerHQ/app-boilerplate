// Auto-generated fixtures for ALONZO_CIP36 era transaction tests
// Generated from LedgerJS signTx.ts test cases
//
// Total tests: 6

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "test_fixture_types.h"

// ======================================================================
// Fixtures
// ======================================================================

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverlength-strings"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverlength-strings"
#endif

// Test 0: Sign_tx_with_CIP36_registration_with_vote_key_hex
//
static const uint8_t FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_VOTE_KEY_HEX_RAW_TX[] = {
    0x3B, 0x40, 0x26, 0x51, 0x11, 0xD8, 0xBB, 0x3C, 0x3C, 0x60, 0x8D, 0x95, 0xB3, 0xA0, 0xBF, 0x83,
    0x46, 0x1A, 0xCE, 0x32, 0xD7, 0x93, 0x36, 0x57, 0x9A, 0x19, 0x39, 0xB3, 0xAA, 0xD1, 0xC0, 0xB7,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x02, 0x00, 0x01, 0x05, 0x80, 0x00, 0x07, 0x3C, 0x80, 0x00,
    0x07, 0x17, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x05,
    0x80, 0x00, 0x07, 0x3C, 0x80, 0x00, 0x07, 0x17, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6C, 0xA7, 0x93, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A,
};

static const tx_fixture_t FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_VOTE_KEY_HEX = {
    .name = "Sign_tx_with_CIP36_registration_with_vote_key_hex",
    .raw_tx = FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_VOTE_KEY_HEX_RAW_TX,
    .raw_tx_len = sizeof(FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_VOTE_KEY_HEX_RAW_TX),
    .tx_body_cbor_hex = "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70001818258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca79302182a030a0758201999b3bb9102b585c42616e40cf1290518d788f967ab4b3329dcb712ac933da0",
    .expected_hash_hex = "358f273c7416fba90abaec19dfa96eb7257ffd047edcb8f035acb0403bd3c6ce",
    .signing_mode = 3,
    .network_id = 1,
    .protocol_magic = 764824073,
    .num_inputs = 1,
    .num_outputs = 1,
    .num_witnesses = 1,
    .num_certificates = 0,
    .num_withdrawals = 0,
    .num_mint_asset_groups = 0,
    .include_ttl = true,
    .include_validity_interval_start = false,
    .include_aux_data_hash = false,
    .include_script_data_hash = false,
    .num_collateral_inputs = 0,
    .num_required_signers = 0,
    .include_network_id = false,
    .include_collateral_output = false,
    .include_total_collateral = false,
    .num_reference_inputs = 0,
    .num_voters = 0,
    .include_treasury = false,
    .treasury = 0,
    .include_donation = false,
    .donation = 0,
    .aux_data_hash_hex = NULL,
    .options = 0,
};

// Test 1: Sign_tx_with_CIP36_registration_with_vote_key_path
//
static const uint8_t FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_VOTE_KEY_PATH_RAW_TX[] = {
    0x3B, 0x40, 0x26, 0x51, 0x11, 0xD8, 0xBB, 0x3C, 0x3C, 0x60, 0x8D, 0x95, 0xB3, 0xA0, 0xBF, 0x83,
    0x46, 0x1A, 0xCE, 0x32, 0xD7, 0x93, 0x36, 0x57, 0x9A, 0x19, 0x39, 0xB3, 0xAA, 0xD1, 0xC0, 0xB7,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x02, 0x00, 0x01, 0x05, 0x80, 0x00, 0x07, 0x3C, 0x80, 0x00,
    0x07, 0x17, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x05,
    0x80, 0x00, 0x07, 0x3C, 0x80, 0x00, 0x07, 0x17, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6C, 0xA7, 0x93, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
};

static const tx_fixture_t FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_VOTE_KEY_PATH = {
    .name = "Sign_tx_with_CIP36_registration_with_vote_key_path",
    .raw_tx = FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_VOTE_KEY_PATH_RAW_TX,
    .raw_tx_len = sizeof(FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_VOTE_KEY_PATH_RAW_TX),
    .tx_body_cbor_hex = "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70001818258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca79302182a030a075820d05698c555a117014a3b360a66931ec43bf18e2aa16560fc99dbd92dd7f6f6540807",
    .expected_hash_hex = "7244322ab32df88ab579dd67da9f77fe172129059ed8c8896dddb35573ee3dcd",
    .signing_mode = 3,
    .network_id = 1,
    .protocol_magic = 764824073,
    .num_inputs = 1,
    .num_outputs = 1,
    .num_witnesses = 1,
    .num_certificates = 0,
    .num_withdrawals = 0,
    .num_mint_asset_groups = 0,
    .include_ttl = true,
    .include_validity_interval_start = true,
    .include_aux_data_hash = false,
    .include_script_data_hash = false,
    .num_collateral_inputs = 0,
    .num_required_signers = 0,
    .include_network_id = false,
    .include_collateral_output = false,
    .include_total_collateral = false,
    .num_reference_inputs = 0,
    .num_voters = 0,
    .include_treasury = false,
    .treasury = 0,
    .include_donation = false,
    .donation = 0,
    .aux_data_hash_hex = NULL,
    .options = 0,
};

// Test 2: Sign_tx_with_CIP36_registration_with_unusual_vote_key_path
//
static const uint8_t FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_UNUSUAL_VOTE_KEY_PATH_RAW_TX[] = {
    0x3B, 0x40, 0x26, 0x51, 0x11, 0xD8, 0xBB, 0x3C, 0x3C, 0x60, 0x8D, 0x95, 0xB3, 0xA0, 0xBF, 0x83,
    0x46, 0x1A, 0xCE, 0x32, 0xD7, 0x93, 0x36, 0x57, 0x9A, 0x19, 0x39, 0xB3, 0xAA, 0xD1, 0xC0, 0xB7,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x02, 0x00, 0x01, 0x05, 0x80, 0x00, 0x07, 0x3C, 0x80, 0x00,
    0x07, 0x17, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x05,
    0x80, 0x00, 0x07, 0x3C, 0x80, 0x00, 0x07, 0x17, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6C, 0xA7, 0x93, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
};

static const tx_fixture_t FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_UNUSUAL_VOTE_KEY_PATH = {
    .name = "Sign_tx_with_CIP36_registration_with_unusual_vote_key_path",
    .raw_tx = FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_UNUSUAL_VOTE_KEY_PATH_RAW_TX,
    .raw_tx_len = sizeof(FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_UNUSUAL_VOTE_KEY_PATH_RAW_TX),
    .tx_body_cbor_hex = "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70001818258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca79302182a030a07582077be323b8df4c6aa1bf2f180112f85ffe8d7f658bc8febdf7dbd5a07453a31cb0807",
    .expected_hash_hex = "34e9f85a4af9487bdf31de7d01132f63f8b3461cd8ec751851188cff7b3ee7bb",
    .signing_mode = 3,
    .network_id = 1,
    .protocol_magic = 764824073,
    .num_inputs = 1,
    .num_outputs = 1,
    .num_witnesses = 1,
    .num_certificates = 0,
    .num_withdrawals = 0,
    .num_mint_asset_groups = 0,
    .include_ttl = true,
    .include_validity_interval_start = true,
    .include_aux_data_hash = false,
    .include_script_data_hash = false,
    .num_collateral_inputs = 0,
    .num_required_signers = 0,
    .include_network_id = false,
    .include_collateral_output = false,
    .include_total_collateral = false,
    .num_reference_inputs = 0,
    .num_voters = 0,
    .include_treasury = false,
    .treasury = 0,
    .include_donation = false,
    .donation = 0,
    .aux_data_hash_hex = NULL,
    .options = 0,
};

// Test 3: Sign_tx_with_CIP36_registration_with_thirdparty_payment_address
//
static const uint8_t FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_THIRDPARTY_PAYMENT_ADDRESS_RAW_TX[] = {
    0x3B, 0x40, 0x26, 0x51, 0x11, 0xD8, 0xBB, 0x3C, 0x3C, 0x60, 0x8D, 0x95, 0xB3, 0xA0, 0xBF, 0x83,
    0x46, 0x1A, 0xCE, 0x32, 0xD7, 0x93, 0x36, 0x57, 0x9A, 0x19, 0x39, 0xB3, 0xAA, 0xD1, 0xC0, 0xB7,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x02, 0x00, 0x01, 0x05, 0x80, 0x00, 0x07, 0x3C, 0x80, 0x00,
    0x07, 0x17, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x05,
    0x80, 0x00, 0x07, 0x3C, 0x80, 0x00, 0x07, 0x17, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6C, 0xA7, 0x93, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
};

static const tx_fixture_t FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_THIRDPARTY_PAYMENT_ADDRESS = {
    .name = "Sign_tx_with_CIP36_registration_with_thirdparty_payment_address",
    .raw_tx = FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_THIRDPARTY_PAYMENT_ADDRESS_RAW_TX,
    .raw_tx_len = sizeof(FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_THIRDPARTY_PAYMENT_ADDRESS_RAW_TX),
    .tx_body_cbor_hex = "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70001818258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca79302182a030a07582042e408fb03986a958be9e2cca01623a31e23f86f31172a5a9b84acdfce6f0e750807",
    .expected_hash_hex = "69ce56529386b4a68138c5f5a0063758c3ff09d9c53cd075943e37a43f236805",
    .signing_mode = 3,
    .network_id = 1,
    .protocol_magic = 764824073,
    .num_inputs = 1,
    .num_outputs = 1,
    .num_witnesses = 1,
    .num_certificates = 0,
    .num_withdrawals = 0,
    .num_mint_asset_groups = 0,
    .include_ttl = true,
    .include_validity_interval_start = true,
    .include_aux_data_hash = false,
    .include_script_data_hash = false,
    .num_collateral_inputs = 0,
    .num_required_signers = 0,
    .include_network_id = false,
    .include_collateral_output = false,
    .include_total_collateral = false,
    .num_reference_inputs = 0,
    .num_voters = 0,
    .include_treasury = false,
    .treasury = 0,
    .include_donation = false,
    .donation = 0,
    .aux_data_hash_hex = NULL,
    .options = 0,
};

// Test 4: Sign_tx_with_CIP36_registration_with_voting_purpose
//
static const uint8_t FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_VOTING_PURPOSE_RAW_TX[] = {
    0x3B, 0x40, 0x26, 0x51, 0x11, 0xD8, 0xBB, 0x3C, 0x3C, 0x60, 0x8D, 0x95, 0xB3, 0xA0, 0xBF, 0x83,
    0x46, 0x1A, 0xCE, 0x32, 0xD7, 0x93, 0x36, 0x57, 0x9A, 0x19, 0x39, 0xB3, 0xAA, 0xD1, 0xC0, 0xB7,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x02, 0x00, 0x01, 0x05, 0x80, 0x00, 0x07, 0x3C, 0x80, 0x00,
    0x07, 0x17, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x05,
    0x80, 0x00, 0x07, 0x3C, 0x80, 0x00, 0x07, 0x17, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6C, 0xA7, 0x93, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
};

static const tx_fixture_t FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_VOTING_PURPOSE = {
    .name = "Sign_tx_with_CIP36_registration_with_voting_purpose",
    .raw_tx = FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_VOTING_PURPOSE_RAW_TX,
    .raw_tx_len = sizeof(FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_VOTING_PURPOSE_RAW_TX),
    .tx_body_cbor_hex = "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70001818258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca79302182a030a075820d706aed1ebc1e8af188aae6d37ffdf4e259a0f04635bef5edce7f43ff632c4450807",
    .expected_hash_hex = "0791e4d8da98cacbc4bbda3bc5ed24bc9e9ed40e73ae3e785b2d23029176aeb8",
    .signing_mode = 3,
    .network_id = 1,
    .protocol_magic = 764824073,
    .num_inputs = 1,
    .num_outputs = 1,
    .num_witnesses = 1,
    .num_certificates = 0,
    .num_withdrawals = 0,
    .num_mint_asset_groups = 0,
    .include_ttl = true,
    .include_validity_interval_start = true,
    .include_aux_data_hash = false,
    .include_script_data_hash = false,
    .num_collateral_inputs = 0,
    .num_required_signers = 0,
    .include_network_id = false,
    .include_collateral_output = false,
    .include_total_collateral = false,
    .num_reference_inputs = 0,
    .num_voters = 0,
    .include_treasury = false,
    .treasury = 0,
    .include_donation = false,
    .donation = 0,
    .aux_data_hash_hex = NULL,
    .options = 0,
};

// Test 5: Sign_tx_with_CIP36_registration_with_delegations
//
static const uint8_t FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_DELEGATIONS_RAW_TX[] = {
    0x3B, 0x40, 0x26, 0x51, 0x11, 0xD8, 0xBB, 0x3C, 0x3C, 0x60, 0x8D, 0x95, 0xB3, 0xA0, 0xBF, 0x83,
    0x46, 0x1A, 0xCE, 0x32, 0xD7, 0x93, 0x36, 0x57, 0x9A, 0x19, 0x39, 0xB3, 0xAA, 0xD1, 0xC0, 0xB7,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x02, 0x00, 0x01, 0x05, 0x80, 0x00, 0x07, 0x3C, 0x80, 0x00,
    0x07, 0x17, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x05,
    0x80, 0x00, 0x07, 0x3C, 0x80, 0x00, 0x07, 0x17, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6C, 0xA7, 0x93, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
};

static const tx_fixture_t FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_DELEGATIONS = {
    .name = "Sign_tx_with_CIP36_registration_with_delegations",
    .raw_tx = FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_DELEGATIONS_RAW_TX,
    .raw_tx_len = sizeof(FIXTURE_ALONZO_CIP36_SIGN_TX_WITH_CIP36_REGISTRATION_WITH_DELEGATIONS_RAW_TX),
    .tx_body_cbor_hex = "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70001818258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca79302182a030a075820f0e62a047ef597d9fb1bfefb9cd3f4e77558c33510ca552484ee8b5c77bbdf650807",
    .expected_hash_hex = "7cdb049d053e7e957d42128f7d90bde2b7f928853bb97ee2f935899f7b521cb8",
    .signing_mode = 3,
    .network_id = 1,
    .protocol_magic = 764824073,
    .num_inputs = 1,
    .num_outputs = 1,
    .num_witnesses = 1,
    .num_certificates = 0,
    .num_withdrawals = 0,
    .num_mint_asset_groups = 0,
    .include_ttl = true,
    .include_validity_interval_start = true,
    .include_aux_data_hash = false,
    .include_script_data_hash = false,
    .num_collateral_inputs = 0,
    .num_required_signers = 0,
    .include_network_id = false,
    .include_collateral_output = false,
    .include_total_collateral = false,
    .num_reference_inputs = 0,
    .num_voters = 0,
    .include_treasury = false,
    .treasury = 0,
    .include_donation = false,
    .donation = 0,
    .aux_data_hash_hex = NULL,
    .options = 0,
};

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
