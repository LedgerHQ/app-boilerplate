// Auto-generated fixtures for ALONZO_CATALYST era transaction tests
// Generated from LedgerJS signTx.ts test cases
//
// Total tests: 2

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

// Test 0: Sign_tx_with_Catalyst_registration_metadata_with_base_address
//
static const uint8_t FIXTURE_ALONZO_CATALYST_SIGN_TX_WITH_CATALYST_REGISTRATION_METADATA_WITH_BASE_ADDRESS_RAW_TX[] = {
    0x3B, 0x40, 0x26, 0x51, 0x11, 0xD8, 0xBB, 0x3C, 0x3C, 0x60, 0x8D, 0x95, 0xB3, 0xA0, 0xBF, 0x83,
    0x46, 0x1A, 0xCE, 0x32, 0xD7, 0x93, 0x36, 0x57, 0x9A, 0x19, 0x39, 0xB3, 0xAA, 0xD1, 0xC0, 0xB7,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x02, 0x00, 0x01, 0x05, 0x80, 0x00, 0x07, 0x3C, 0x80, 0x00,
    0x07, 0x17, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x05,
    0x80, 0x00, 0x07, 0x3C, 0x80, 0x00, 0x07, 0x17, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6C, 0xA7, 0x93, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
};

static const tx_fixture_t FIXTURE_ALONZO_CATALYST_SIGN_TX_WITH_CATALYST_REGISTRATION_METADATA_WITH_BASE_ADDRESS = {
    .name = "Sign_tx_with_Catalyst_registration_metadata_with_base_address",
    .raw_tx = FIXTURE_ALONZO_CATALYST_SIGN_TX_WITH_CATALYST_REGISTRATION_METADATA_WITH_BASE_ADDRESS_RAW_TX,
    .raw_tx_len = sizeof(FIXTURE_ALONZO_CATALYST_SIGN_TX_WITH_CATALYST_REGISTRATION_METADATA_WITH_BASE_ADDRESS_RAW_TX),
    .tx_body_cbor_hex = "a600818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70001818258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca79302182a030a075820e9141b460aea0abb69ce113c7302c7c03690267736d6a382ee62d2a53c2ec9260807",
    .expected_hash_hex = "9941060a76f5702e72b43c382f77b143ed0e328ac3977a0791f08a5f0e0149cd",
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

// Test 1: Sign_tx_with_Catalyst_registration_metadata_with_stake_address
//
static const uint8_t FIXTURE_ALONZO_CATALYST_SIGN_TX_WITH_CATALYST_REGISTRATION_METADATA_WITH_STAKE_ADDRESS_RAW_TX[] = {
    0x3B, 0x40, 0x26, 0x51, 0x11, 0xD8, 0xBB, 0x3C, 0x3C, 0x60, 0x8D, 0x95, 0xB3, 0xA0, 0xBF, 0x83,
    0x46, 0x1A, 0xCE, 0x32, 0xD7, 0x93, 0x36, 0x57, 0x9A, 0x19, 0x39, 0xB3, 0xAA, 0xD1, 0xC0, 0xB7,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x3B, 0x02, 0x00, 0x01, 0x05, 0x80, 0x00, 0x07, 0x3C, 0x80, 0x00,
    0x07, 0x17, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x05,
    0x80, 0x00, 0x07, 0x3C, 0x80, 0x00, 0x07, 0x17, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6C, 0xA7, 0x93, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0A,
};

static const tx_fixture_t FIXTURE_ALONZO_CATALYST_SIGN_TX_WITH_CATALYST_REGISTRATION_METADATA_WITH_STAKE_ADDRESS = {
    .name = "Sign_tx_with_Catalyst_registration_metadata_with_stake_address",
    .raw_tx = FIXTURE_ALONZO_CATALYST_SIGN_TX_WITH_CATALYST_REGISTRATION_METADATA_WITH_STAKE_ADDRESS_RAW_TX,
    .raw_tx_len = sizeof(FIXTURE_ALONZO_CATALYST_SIGN_TX_WITH_CATALYST_REGISTRATION_METADATA_WITH_STAKE_ADDRESS_RAW_TX),
    .tx_body_cbor_hex = "a500818258203b40265111d8bb3c3c608d95b3a0bf83461ace32d79336579a1939b3aad1c0b70001818258390114c16d7f43243bd81478e68b9db53a8528fd4fb1078d58d54a7f11241d227aefa4b773149170885aadba30aab3127cc611ddbc4999def61c1a006ca79302182a030a075820d19f7cb4d48a6ae8d370c64d2a42fca1f61d6b2cf3d0c0c02801541811338deb",
    .expected_hash_hex = "90ab18ad3a25cb9f48470cb16a51e1fe04181b96f639d939c51ca81ab4c0fa23",
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

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
