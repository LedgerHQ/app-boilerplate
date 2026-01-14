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
#include "cardano_settings.h"
#include "cardano_constants.h"
#include "test_fixture_types.h"
#include "apdu/dispatcher.h"

extern bool app_mem_init(void);
extern bool unit_test_expert_mode_enabled;
static inline void reset_context(void) {
    memset(&G_context, 0, sizeof(G_context));
    g_last_response_len = 0;
    g_last_response_sw = 0;
}

static inline void run_tx_and_verify(const uint8_t* init_raw,
                                     size_t init_len,
                                     const uint8_t* raw_tx,
                                     size_t raw_tx_len,
                                     bool include_aux_data_hash,
                                     uint8_t aux_data_type,
                                     const uint8_t* aux_data_init_payload,
                                     size_t aux_data_init_payload_len,
                                     const aux_data_payload_t* aux_data_delegations,
                                     size_t aux_data_delegation_count,
                                     const char* cbor_hex,
                                     const char* expected_hash_hex,
                                     uint16_t num_witnesses,
                                     bool include_ttl,
                                     bool include_validity_interval_start,
                                     const uint8_t* response_buf,
                                     size_t* response_len,
                                     uint16_t* response_sw) {
    assert_true(init_len > 0);
    handler_sign_tx(&(buffer_t){.ptr = (uint8_t*)init_raw, .size = init_len, .offset = 0}, 0x00, false);
    assert_int_equal(G_context.req_type, REQUEST_SIGN_TRANSACTION);
    if (include_aux_data_hash && aux_data_type == AUX_DATA_TYPE_CVOTE_REGISTRATION) {
        assert_int_equal(G_context.state.tx_state, TX_STATE_AUX_DATA);
    } else {
        assert_int_equal(G_context.state.tx_state, TX_STATE_CHUNKS);
    }
    assert_int_equal(G_context.tx_info.num_witnesses, num_witnesses);
    assert_int_equal(G_context.tx_info.transaction.includeTtl, include_ttl);
    assert_int_equal(G_context.tx_info.transaction.includeValidityIntervalStart, include_validity_interval_start);

    if (include_aux_data_hash && aux_data_type == AUX_DATA_TYPE_CVOTE_REGISTRATION) {
        assert_non_null(aux_data_init_payload);
        assert_true(aux_data_init_payload_len > 0);
        buffer_t aux_init_buf = {
            .ptr = (uint8_t*) aux_data_init_payload,
            .size = aux_data_init_payload_len,
            .offset = 0,
        };
        handler_sign_tx_aux_data(&aux_init_buf, P2_AUX_DATA_INIT);

        for (size_t i = 0; i < aux_data_delegation_count; i++) {
            const aux_data_payload_t* delegation = &aux_data_delegations[i];
            assert_non_null(delegation->payload);
            assert_true(delegation->payload_len > 0);
            buffer_t aux_reg_buf = {
                .ptr = (uint8_t*) delegation->payload,
                .size = delegation->payload_len,
                .offset = 0,
            };
            handler_sign_tx_aux_data(&aux_reg_buf, P2_AUX_DATA_DELEGATION);
        }

        assert_int_equal(G_context.state.tx_state, TX_STATE_CHUNKS);
    }

    buffer_t tx_buf = {
        .ptr = (uint8_t*) raw_tx,
        .size = raw_tx_len,
        .offset = 0,
    };
    handler_sign_tx(&tx_buf, 0x02, false);

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
    if (fixture->include_aux_data_hash && fixture->aux_data_type == AUX_DATA_TYPE_ARBITRARY_HASH) {
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
        .auxDataType = fixture->aux_data_type,
        .auxDataHash = (fixture->include_aux_data_hash &&
                        fixture->aux_data_type == AUX_DATA_TYPE_ARBITRARY_HASH) ? aux_data_hash : NULL,
        .auxDataHashLen = (fixture->include_aux_data_hash &&
                           fixture->aux_data_type == AUX_DATA_TYPE_ARBITRARY_HASH) ? aux_hash_len : 0,
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
                      fixture->include_aux_data_hash,
                      fixture->aux_data_type,
                      fixture->aux_data_init_payload,
                      fixture->aux_data_init_payload_len,
                      fixture->aux_data_delegations,
                      fixture->aux_data_delegation_count,
                      fixture->tx_body_cbor_hex,
                      fixture->expected_hash_hex,
                      fixture->num_witnesses,
                      fixture->include_ttl,
                      fixture->include_validity_interval_start,
                      g_last_response,
                      &g_last_response_len,
                      &g_last_response_sw);
}

static inline void run_fixture_with_expert_mode(const tx_fixture_t *fixture, bool expert_mode) {
    extern bool unit_test_expert_mode_enabled;
    const bool previous_mode = unit_test_expert_mode_enabled;
    unit_test_expert_mode_enabled = expert_mode;
    run_fixture(fixture);
    unit_test_expert_mode_enabled = previous_mode;
}
