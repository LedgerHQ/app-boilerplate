// Auto-generated fixtures for CONWAY_WITHOUT_CERTIFICATES era transaction tests
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

// Test 0: Sign_tx_with_treasury
//
static const uint8_t FIXTURE_CONWAY_WITHOUT_CERTIFICATES_SIGN_TX_WITH_TREASURY_RAW_TX[] = {
    0x3B, 0x40, 0x26, 0x51, 0x11, 0xD8, 0xBB, 0x3C, 0x3C, 0x60, 0x8D, 0x95, 0xB3, 0xA0, 0xBF, 0x83,
    0x46, 0x1A, 0xCE, 0x32, 0xD7, 0x93, 0x36, 0x57, 0x9A, 0x19, 0x39, 0xB3, 0xAA, 0xD1, 0xC0, 0xB7,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x01, 0x00, 0x2B, 0x82, 0xD8, 0x18, 0x58, 0x21, 0x83, 0x58,
    0x1C, 0x9E, 0x1C, 0x71, 0xDE, 0x65, 0x2E, 0xC8, 0xB8, 0x5F, 0xEC, 0x29, 0x6F, 0x06, 0x85, 0xCA,
    0x39, 0x88, 0x78, 0x1C, 0x94, 0xA2, 0xE1, 0xA5, 0xD8, 0x9D, 0x92, 0xF4, 0x5F, 0xA0, 0x00, 0x1A,
    0x0D, 0x0C, 0x25, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2D, 0xD2, 0xE8, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1B,
};

static const tx_fixture_t FIXTURE_CONWAY_WITHOUT_CERTIFICATES_SIGN_TX_WITH_TREASURY = {
    .name = "Sign_tx_with_treasury",
    .raw_tx = FIXTURE_CONWAY_WITHOUT_CERTIFICATES_SIGN_TX_WITH_TREASURY_RAW_TX,
    .raw_tx_len = sizeof(FIXTURE_CONWAY_WITHOUT_CERTIFICATES_SIGN_TX_WITH_TREASURY_RAW_TX),
    .tx_body_cbor_hex = "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a15181b",
    .expected_hash_hex = "c3fb3f4330d9f051b567db11d56ab8174cba081373936c9fde80d809b178326f",
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
    .aux_data_type = 0,
    .aux_data_init_payload = NULL,
    .aux_data_init_payload_len = 0,
    .aux_data_delegations = NULL,
    .aux_data_delegation_count = 0,
    .include_script_data_hash = false,
    .num_collateral_inputs = 0,
    .num_required_signers = 0,
    .include_network_id = false,
    .include_collateral_output = false,
    .include_total_collateral = false,
    .num_reference_inputs = 0,
    .num_voters = 0,
    .include_treasury = true,
    .treasury = 27,
    .include_donation = false,
    .donation = 0,
    .aux_data_hash_hex = NULL,
    .options = 0,
};

// Test 1: Sign_tx_with_donation
//
static const uint8_t FIXTURE_CONWAY_WITHOUT_CERTIFICATES_SIGN_TX_WITH_DONATION_RAW_TX[] = {
    0x3B, 0x40, 0x26, 0x51, 0x11, 0xD8, 0xBB, 0x3C, 0x3C, 0x60, 0x8D, 0x95, 0xB3, 0xA0, 0xBF, 0x83,
    0x46, 0x1A, 0xCE, 0x32, 0xD7, 0x93, 0x36, 0x57, 0x9A, 0x19, 0x39, 0xB3, 0xAA, 0xD1, 0xC0, 0xB7,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x01, 0x00, 0x2B, 0x82, 0xD8, 0x18, 0x58, 0x21, 0x83, 0x58,
    0x1C, 0x9E, 0x1C, 0x71, 0xDE, 0x65, 0x2E, 0xC8, 0xB8, 0x5F, 0xEC, 0x29, 0x6F, 0x06, 0x85, 0xCA,
    0x39, 0x88, 0x78, 0x1C, 0x94, 0xA2, 0xE1, 0xA5, 0xD8, 0x9D, 0x92, 0xF4, 0x5F, 0xA0, 0x00, 0x1A,
    0x0D, 0x0C, 0x25, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2D, 0xD2, 0xE8, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C,
};

static const tx_fixture_t FIXTURE_CONWAY_WITHOUT_CERTIFICATES_SIGN_TX_WITH_DONATION = {
    .name = "Sign_tx_with_donation",
    .raw_tx = FIXTURE_CONWAY_WITHOUT_CERTIFICATES_SIGN_TX_WITH_DONATION_RAW_TX,
    .raw_tx_len = sizeof(FIXTURE_CONWAY_WITHOUT_CERTIFICATES_SIGN_TX_WITH_DONATION_RAW_TX),
    .tx_body_cbor_hex = "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a16181c",
    .expected_hash_hex = "618718225c0c876fd429ca25957a23ad894eaebc1831365c5b41cba50993a1bf",
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
    .aux_data_type = 0,
    .aux_data_init_payload = NULL,
    .aux_data_init_payload_len = 0,
    .aux_data_delegations = NULL,
    .aux_data_delegation_count = 0,
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
    .include_donation = true,
    .donation = 28,
    .aux_data_hash_hex = NULL,
    .options = 0,
};

// Test 2: Sign_tx_with_treasury_and_donation
//
static const uint8_t FIXTURE_CONWAY_WITHOUT_CERTIFICATES_SIGN_TX_WITH_TREASURY_AND_DONATION_RAW_TX[] = {
    0x3B, 0x40, 0x26, 0x51, 0x11, 0xD8, 0xBB, 0x3C, 0x3C, 0x60, 0x8D, 0x95, 0xB3, 0xA0, 0xBF, 0x83,
    0x46, 0x1A, 0xCE, 0x32, 0xD7, 0x93, 0x36, 0x57, 0x9A, 0x19, 0x39, 0xB3, 0xAA, 0xD1, 0xC0, 0xB7,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x01, 0x00, 0x2B, 0x82, 0xD8, 0x18, 0x58, 0x21, 0x83, 0x58,
    0x1C, 0x9E, 0x1C, 0x71, 0xDE, 0x65, 0x2E, 0xC8, 0xB8, 0x5F, 0xEC, 0x29, 0x6F, 0x06, 0x85, 0xCA,
    0x39, 0x88, 0x78, 0x1C, 0x94, 0xA2, 0xE1, 0xA5, 0xD8, 0x9D, 0x92, 0xF4, 0x5F, 0xA0, 0x00, 0x1A,
    0x0D, 0x0C, 0x25, 0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2D, 0xD2, 0xE8, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x1C,
};

static const tx_fixture_t FIXTURE_CONWAY_WITHOUT_CERTIFICATES_SIGN_TX_WITH_TREASURY_AND_DONATION = {
    .name = "Sign_tx_with_treasury_and_donation",
    .raw_tx = FIXTURE_CONWAY_WITHOUT_CERTIFICATES_SIGN_TX_WITH_TREASURY_AND_DONATION_RAW_TX,
    .raw_tx_len = sizeof(FIXTURE_CONWAY_WITHOUT_CERTIFICATES_SIGN_TX_WITH_TREASURY_AND_DONATION_RAW_TX),
    .tx_body_cbor_hex = "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b700018182582b82d818582183581c9e1c71de652ec8b85fec296f0685ca3988781c94a2e1a5d89d92f45fa0001a0d0c25611a002dd2e802182a030a15181b16181c",
    .expected_hash_hex = "37e2cf72599186d1ec571af568ea0ab39f7f73ba2e8c5f4725bfd0d4fb8c58e7",
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
    .aux_data_type = 0,
    .aux_data_init_payload = NULL,
    .aux_data_init_payload_len = 0,
    .aux_data_delegations = NULL,
    .aux_data_delegation_count = 0,
    .include_script_data_hash = false,
    .num_collateral_inputs = 0,
    .num_required_signers = 0,
    .include_network_id = false,
    .include_collateral_output = false,
    .include_total_collateral = false,
    .num_reference_inputs = 0,
    .num_voters = 0,
    .include_treasury = true,
    .treasury = 27,
    .include_donation = true,
    .donation = 28,
    .aux_data_hash_hex = NULL,
    .options = 0,
};

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
