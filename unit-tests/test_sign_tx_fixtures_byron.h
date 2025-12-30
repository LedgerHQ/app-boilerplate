// Auto-generated fixtures for BYRON era transaction tests
// Generated from LedgerJS signTx.ts test cases
//
// Total tests: 3

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

// Test 0: Sign_tx_with_third-party_Byron_mainnet_output
//
static const uint8_t FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_MAINNET_OUTPUT_RAW_TX[] = {
    0x1A, 0xF8, 0xFA, 0x0B, 0x75, 0x4F, 0xF9, 0x92, 0x53, 0xD9, 0x83, 0x89, 0x4E, 0x63, 0xA2, 0xB0,
    0x9C, 0xBB, 0x56, 0xC8, 0x33, 0xBA, 0x18, 0xC3, 0x38, 0x42, 0x10, 0x16, 0x3F, 0x63, 0xDC, 0xFC,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x01, 0x00, 0x2B, 0x82, 0xD8, 0x18, 0x58, 0x21, 0x83, 0x58,
    0x1C, 0x9E, 0x1C, 0x71, 0xDE, 0x65, 0x2E, 0xC8, 0xB8, 0x5F, 0xEC, 0x29, 0x6F, 0x06, 0x85, 0xCA,
    0x39, 0x88, 0x78, 0x1C, 0x94, 0xA2, 0xE1, 0xA5, 0xD8, 0x9D, 0x92, 0xF4, 0x5F, 0xA0, 0x00, 0x1A,
    0x0D, 0x0C, 0x25, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2D, 0xD2, 0xE8, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A,
};

static const tx_fixture_t FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_MAINNET_OUTPUT = {
    .name = "Sign_tx_with_third-party_Byron_mainnet_output",
    .raw_tx = FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_MAINNET_OUTPUT_RAW_TX,
    .raw_tx_len = sizeof(FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_MAINNET_OUTPUT_RAW_TX),
    .tx_body_cbor_hex = "a400818258201af8fa0b754ff99253d983894e63a2b09cbb56c833ba18c3384210163f63dcfc00018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a",
    .expected_hash_hex = "73e09bdebf98a9e0f17f86a2d11e0f14f4f8dae77cdf26ff1678e821f20c8db6",
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

// Test 1: Sign_tx_with_third-party_Byron_Daedalus_mainnet_output
//
static const uint8_t FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_DAEDALUS_MAINNET_OUTPUT_RAW_TX[] = {
    0x1A, 0xF8, 0xFA, 0x0B, 0x75, 0x4F, 0xF9, 0x92, 0x53, 0xD9, 0x83, 0x89, 0x4E, 0x63, 0xA2, 0xB0,
    0x9C, 0xBB, 0x56, 0xC8, 0x33, 0xBA, 0x18, 0xC3, 0x38, 0x42, 0x10, 0x16, 0x3F, 0x63, 0xDC, 0xFC,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x5C, 0x01, 0x00, 0x4C, 0x82, 0xD8, 0x18, 0x58, 0x42, 0x83, 0x58,
    0x1C, 0xD2, 0x34, 0x8B, 0x8E, 0xF7, 0xB8, 0xA6, 0xD1, 0xC9, 0x22, 0xEF, 0xA4, 0x99, 0xC6, 0x69,
    0xB1, 0x51, 0xEE, 0xEF, 0x99, 0xE4, 0xCE, 0x35, 0x21, 0xE8, 0x82, 0x23, 0xF8, 0xA1, 0x01, 0x58,
    0x1E, 0x58, 0x1C, 0xF2, 0x81, 0xE6, 0x48, 0xA8, 0x90, 0x15, 0xA9, 0x86, 0x1B, 0xD9, 0xE9, 0x92,
    0x41, 0x4D, 0x11, 0x45, 0xDD, 0xAF, 0x80, 0x69, 0x0B, 0xE5, 0x32, 0x35, 0xB0, 0xE2, 0xE5, 0x00,
    0x1A, 0x19, 0x98, 0x34, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2D, 0xD2, 0xE8, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x0A,
};

static const tx_fixture_t FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_DAEDALUS_MAINNET_OUTPUT = {
    .name = "Sign_tx_with_third-party_Byron_Daedalus_mainnet_output",
    .raw_tx = FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_DAEDALUS_MAINNET_OUTPUT_RAW_TX,
    .raw_tx_len = sizeof(FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_DAEDALUS_MAINNET_OUTPUT_RAW_TX),
    .tx_body_cbor_hex = "a400818258201af8fa0b754ff99253d983894e63a2b09cbb56c833ba18c3384210163f63dcfc00018182584c82d818584283581cd2348b8ef7b8a6d1c922efa499c669b151eeef99e4ce3521e88223f8a101581e581cf281e648a89015a9861bd9e992414d1145ddaf80690be53235b0e2e5001a199834651a002dd2e802182a030a",
    .expected_hash_hex = "3cf35b4d9bfa87b8eab5de659e0520bdac37b0de0b3840c1d8abd683330a9756",
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

// Test 2: Sign_tx_with_third-party_Byron_testnet_output
//
static const uint8_t FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_TESTNET_OUTPUT_RAW_TX[] = {
    0x1A, 0xF8, 0xFA, 0x0B, 0x75, 0x4F, 0xF9, 0x92, 0x53, 0xD9, 0x83, 0x89, 0x4E, 0x63, 0xA2, 0xB0,
    0x9C, 0xBB, 0x56, 0xC8, 0x33, 0xBA, 0x18, 0xC3, 0x38, 0x42, 0x10, 0x16, 0x3F, 0x63, 0xDC, 0xFC,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x01, 0x00, 0x2F, 0x82, 0xD8, 0x18, 0x58, 0x25, 0x83, 0x58,
    0x1C, 0x70, 0x9B, 0xFB, 0x5D, 0x97, 0x33, 0xCB, 0xDD, 0x72, 0xF5, 0x20, 0xCD, 0x2C, 0x8B, 0x9F,
    0x8F, 0x94, 0x2D, 0xA5, 0xE6, 0xCD, 0x0B, 0x69, 0x94, 0xE1, 0x80, 0x3B, 0x0A, 0xA1, 0x02, 0x42,
    0x18, 0x2A, 0x00, 0x1A, 0xEF, 0x14, 0xE7, 0x6D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2D, 0xD2, 0xE8,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0A,
};

static const tx_fixture_t FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_TESTNET_OUTPUT = {
    .name = "Sign_tx_with_third-party_Byron_testnet_output",
    .raw_tx = FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_TESTNET_OUTPUT_RAW_TX,
    .raw_tx_len = sizeof(FIXTURE_BYRON_SIGN_TX_WITH_THIRD_PARTY_BYRON_TESTNET_OUTPUT_RAW_TX),
    .tx_body_cbor_hex = "a400818258201af8fa0b754ff99253d983894e63a2b09cbb56c833ba18c3384210163f63dcfc00018182582f82d818582583581c709bfb5d9733cbdd72f520cd2c8b9f8f942da5e6cd0b6994e1803b0aa10242182a001aef14e76d1a002dd2e802182a030a",
    .expected_hash_hex = "e2319ee8317ac537af4c2c3322aaf9fb6c64a95e3921ad75ab91b4f5b5306963",
    .signing_mode = 3,
    .network_id = 0,
    .protocol_magic = 42,
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

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
