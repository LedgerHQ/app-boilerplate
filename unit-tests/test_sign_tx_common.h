#pragma once

#include <cmocka.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "buffer.h"
#include "handler/sign_tx.h"
#include "display.h"
#include "hexUtils.h"
#include "transaction/tx.h"
#include "blake2b.h"
#include "globals.h"
#include "cardano_constants.h"
#include "test_fixture_types.h"

extern bool app_mem_init(void);

static inline void reset_context(void) {
    memset(&G_context, 0, sizeof(G_context));
    g_last_response_len = 0;
    g_last_response_sw = 0;
}

static inline void run_tx_and_verify(const uint8_t* init_raw,
                                     size_t init_len,
                                     const uint8_t* raw_tx,
                                     size_t raw_tx_len,
                                     const char* cbor_hex,
                                     const char* expected_hash_hex,
                                     uint16_t num_witnesses,
                                     bool include_ttl,
                                     bool include_validity_interval_start,
                                     const uint8_t* response_buf,
                                     size_t* response_len,
                                     uint16_t* response_sw) {
    assert_true(init_len > 0);
    assert_int_equal(handler_sign_tx(&(buffer_t){.ptr = (uint8_t*)init_raw, .size = init_len, .offset = 0}, 0x00, false), 0);
    assert_int_equal(G_context.req_type, REQUEST_SIGN_TRANSACTION);
    assert_int_equal(G_context.state.tx_state, TX_STATE_CHUNKS);
    assert_int_equal(G_context.tx_info.num_witnesses, num_witnesses);
    assert_int_equal(G_context.tx_info.transaction.includeTtl, include_ttl);
    assert_int_equal(G_context.tx_info.transaction.includeValidityIntervalStart, include_validity_interval_start);

    buffer_t tx_buf = {
        .ptr = (uint8_t*) raw_tx,
        .size = raw_tx_len,
        .offset = 0,
    };
    assert_int_equal(handler_sign_tx(&tx_buf, 0x02, false), 0);

    uint8_t expected_cbor[100 * 1024];
    size_t cbor_len = hex_to_bytes(cbor_hex, expected_cbor, sizeof(expected_cbor));
    assert_true(cbor_len > 0);

    uint8_t expected_hash[TX_HASH_LENGTH];
    size_t expected_hash_len = hex_to_bytes(expected_hash_hex, expected_hash, sizeof(expected_hash));
    assert_int_equal(expected_hash_len, TX_HASH_LENGTH);

    assert_int_equal(*response_len, TX_HASH_LENGTH);
    assert_memory_equal(response_buf, expected_hash, TX_HASH_LENGTH);
    assert_int_equal(*response_sw, SWO_SUCCESS);
    assert_int_equal(G_context.req_type, REQUEST_NONE);

    tx_review_cleanup();
    tx_context_cleanup();
}

static inline void run_fixture(const tx_fixture_t *fixture) {
    reset_context();
    assert_true(app_mem_init());

    uint8_t init_raw[512];
    uint8_t aux_data_hash[AUX_DATA_HASH_LENGTH] = {0};
    size_t aux_hash_len = 0;
    if (fixture->include_aux_data_hash) {
        assert_non_null(fixture->aux_data_hash_hex);
        aux_hash_len = hex_to_bytes(fixture->aux_data_hash_hex, aux_data_hash, sizeof(aux_data_hash));
        assert_int_equal(aux_hash_len, AUX_DATA_HASH_LENGTH);
    }

    init_apdu_params_t params = {
        .options = fixture->options,
        .networkId = fixture->network_id,
        .protocolMagic = fixture->protocol_magic,
        .signingMode = fixture->signing_mode,
        .numInputs = fixture->num_inputs,
        .numOutputs = fixture->num_outputs,
        .includeTtl = fixture->include_ttl,
        .numCertificates = fixture->num_certificates,
        .numWithdrawals = fixture->num_withdrawals,
        .includeAuxData = fixture->include_aux_data_hash,
        .auxDataHash = fixture->include_aux_data_hash ? aux_data_hash : NULL,
        .auxDataHashLen = fixture->include_aux_data_hash ? aux_hash_len : 0,
        .includeScriptDataHash = fixture->include_script_data_hash,
        .includeValidityIntervalStart = fixture->include_validity_interval_start,
        .numMintAssetGroups = fixture->num_mint_asset_groups,
        .numCollateralInputs = fixture->num_collateral_inputs,
        .numRequiredSigners = fixture->num_required_signers,
        .includeNetworkId = fixture->include_network_id,
        .includeCollateralOutput = fixture->include_collateral_output,
        .includeTotalCollateral = fixture->include_total_collateral,
        .numReferenceInputs = fixture->num_reference_inputs,
        .numVoters = fixture->num_voters,
        .includeTreasury = fixture->include_treasury,
        .includeDonation = fixture->include_donation,
        .numWitnesses = fixture->num_witnesses,
    };
    size_t init_len = build_init_apdu(&params, init_raw, sizeof(init_raw));
    assert_true(init_len > 0);

    run_tx_and_verify(init_raw,
                      init_len,
                      fixture->raw_tx,
                      fixture->raw_tx_len,
                      fixture->tx_body_cbor_hex,
                      fixture->expected_hash_hex,
                      fixture->num_witnesses,
                      fixture->include_ttl,
                      fixture->include_validity_interval_start,
                      g_last_response,
                      &g_last_response_len,
                      &g_last_response_sw);
}
