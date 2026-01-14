/*****************************************************************************
 *   Ledger App Cardano.
 *   (c) 2025 Vacuumlabs
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *****************************************************************************/

#include <stdint.h>   // uint*_t
#include <stdbool.h>  // bool
#include <stddef.h>   // size_t
#include <string.h>   // memset, explicit_bzero
#include <stdio.h>    // snprintf

#include "cardano_constants.h"
#include "os.h"
#include "buffer.h"
#include "nbgl_use_case.h"
#include "utils/buffer_utils.h"
#include "io.h"

#include "sign_tx.h"
#include "cardano_swo.h"
#include "globals.h"
#include "ui/ui_display_tx.h"
#include "transaction/tx.h"
#include "transaction/tx_aux_data_types.h"
#include "transaction/tx_credential_types.h"
#include "tx_output_types.h"
#include "tx_parse.h"
#include "memory/mem.h"
#include "utils/utils.h"
#include "utils/textUtils.h"
#include "app_context.h"
#include "utils/cbor.h"
#include "transaction/tx_hash_builder.h"
#include "messageSigning.h"
#include "securityPolicy/securityPolicy.h"
#include "dispatcher.h"
#include "cvote/cvote_parser.h"
#include "addressUtils/bip44.h"
#include "keyDerivation/keyDerivation.h"
#include "cvote/aux_data_hash_builder.h"
#include "transaction/tx_utils.h"
#include "ui/menu.h"
#include "transaction/tx_validate.h"

static bool cvote_aux_data_is_done(void) {
    return G_context.tx_info.cvote_aux_data_expected &&
           G_context.tx_info.cvote_aux_data_initialized &&
           G_context.tx_info.cvote_registrations_remaining == 0;
}

static bool cvote_extract_pubkey(const ext_credential_t *credential, uint8_t *out_pubkey) {
    LEDGER_ASSERT(credential != NULL, "Credential cannot be null");
    LEDGER_ASSERT(out_pubkey != NULL, "Output pubkey buffer cannot be null");

    switch (credential->type) {
        case EXT_CREDENTIAL_KEY_HASH:
            memmove(out_pubkey, credential->publicKey, PUBLIC_KEY_SIZE);
            return true;
        case EXT_CREDENTIAL_KEY_PATH: {
            extendedPublicKey_t derived_key = {0};
            cx_err_t err = deriveExtendedPublicKey(&credential->keyPath, &derived_key);
            if (err != CX_OK) {
                TRACE("Failed to derive CVote key path: 0x%x", err);
                return false;
            }
            memmove(out_pubkey, derived_key.pubKey, PUBLIC_KEY_SIZE);
            return true;
        }
        default:
            TRACE("Unsupported CVote credential type %u", credential->type);
            return false;
    }
}

static bool cvote_extract_destination_address(const cvote_destination_t *destination,
                                              uint8_t *address_buffer,
                                              size_t *out_len) {
    LEDGER_ASSERT(destination != NULL, "Destination cannot be null");
    LEDGER_ASSERT(address_buffer != NULL, "Address buffer cannot be null");
    LEDGER_ASSERT(out_len != NULL, "Output length pointer cannot be null");

    if (destination->is_third_party) {
        if (destination->third_party.length == 0 || destination->third_party.buffer == NULL) {
            TRACE("CVote third-party destination empty");
            return false;
        }
        memmove(address_buffer, destination->third_party.buffer, destination->third_party.length);
        *out_len = destination->third_party.length;
        return true;
    }

    size_t address_size = deriveAddress(&destination->params, address_buffer, MAX_ADDRESS_LENGTH);
    if (address_size == 0 || address_size > MAX_ADDRESS_LENGTH) {
        TRACE("CVote destination address derivation failed (%u)", (unsigned) address_size);
        return false;
    }
    *out_len = address_size;
    return true;
}

static void cvote_hash_builder_setup(cvote_aux_data_t *aux_data) {
    LEDGER_ASSERT(aux_data != NULL, "Auxiliary data cannot be null");

    auxDataHashBuilder_init(&aux_data->hash_builder);
    auxDataHashBuilder_cVoteRegistration_enter(&aux_data->hash_builder, aux_data->format);
    auxDataHashBuilder_cVoteRegistration_enterPayload(&aux_data->hash_builder);
    if (aux_data->format == CIP36 && aux_data->delegation_count > 0) {
        auxDataHashBuilder_cVoteRegistration_enterDelegations(&aux_data->hash_builder,
                                                              aux_data->delegation_count);
    }
}

static bool cvote_hash_builder_add_vote_key(cvote_aux_data_t *aux_data) {
    LEDGER_ASSERT(aux_data != NULL, "Auxiliary data cannot be null");

    if (aux_data->format == CIP36 && aux_data->delegation_count > 0) {
        return true;
    }
    if (aux_data->format != CIP15 && aux_data->format != CIP36) {
        return true;
    }

    if (!aux_data->has_vote_credential) {
        TRACE("CVote vote key missing");
        return false;
    }
    uint8_t pubkey[PUBLIC_KEY_SIZE] = {0};
    if (!cvote_extract_pubkey(&aux_data->vote_credential, pubkey)) {
        return false;
    }
    auxDataHashBuilder_cVoteRegistration_addVoteKey(&aux_data->hash_builder, pubkey, sizeof(pubkey));
    return true;
}

static bool cvote_hash_builder_add_staking_key(cvote_aux_data_t *aux_data) {
    LEDGER_ASSERT(aux_data != NULL, "Auxiliary data cannot be null");

    uint8_t pubkey[PUBLIC_KEY_SIZE] = {0};
    if (!cvote_extract_pubkey(&aux_data->staking_credential, pubkey)) {
        return false;
    }
    auxDataHashBuilder_cVoteRegistration_addStakingKey(&aux_data->hash_builder, pubkey, sizeof(pubkey));
    return true;
}

static bool cvote_hash_builder_add_payment_address(cvote_aux_data_t *aux_data) {
    LEDGER_ASSERT(aux_data != NULL, "Auxiliary data cannot be null");

    uint8_t address_buffer[MAX_ADDRESS_LENGTH] = {0};
    size_t address_len = 0;
    if (!cvote_extract_destination_address(&aux_data->destination, address_buffer, &address_len)) {
        return false;
    }
    auxDataHashBuilder_cVoteRegistration_addPaymentAddress(&aux_data->hash_builder,
                                                          address_buffer,
                                                          address_len);
    return true;
}

static bool cvote_hash_builder_add_nonce(cvote_aux_data_t *aux_data) {
    LEDGER_ASSERT(aux_data != NULL, "Auxiliary data cannot be null");

    auxDataHashBuilder_cVoteRegistration_addNonce(&aux_data->hash_builder, aux_data->nonce);
    return true;
}

static bool cvote_hash_builder_add_voting_purpose(cvote_aux_data_t *aux_data) {
    LEDGER_ASSERT(aux_data != NULL, "Auxiliary data cannot be null");

    if (!aux_data->has_voting_purpose) {
        return true;
    }
    auxDataHashBuilder_cVoteRegistration_addVotingPurpose(&aux_data->hash_builder, aux_data->voting_purpose);
    return true;
}

static bool cvote_hash_builder_add_common_fields(cvote_aux_data_t *aux_data) {
    LEDGER_ASSERT(aux_data != NULL, "Auxiliary data cannot be null");

    if (aux_data->final_fields_processed) {
        return true;
    }

    if (!cvote_hash_builder_add_vote_key(aux_data)) {
        return false;
    }
    if (!cvote_hash_builder_add_staking_key(aux_data)) {
        return false;
    }
    if (!cvote_hash_builder_add_payment_address(aux_data)) {
        return false;
    }
    if (!cvote_hash_builder_add_nonce(aux_data)) {
        return false;
    }
    if (aux_data->format == CIP36) {
        if (!cvote_hash_builder_add_voting_purpose(aux_data)) {
            return false;
        }
    }
    aux_data->final_fields_processed = true;
    return true;
}

static bool cvote_append_registration_signature(cvote_aux_data_t *aux_data) {
    LEDGER_ASSERT(aux_data != NULL, "Auxiliary data cannot be null");

    if (aux_data->staking_credential.type != EXT_CREDENTIAL_KEY_PATH) {
        TRACE("CVote staking credential is not a key path");
        return false;
    }

    security_policy_t policy =
        policyForCVoteRegistrationStakingKey(&aux_data->staking_credential.keyPath);
    if (policy == POLICY_DENY) {
        TRACE("CVote staking key policy denied");
        return false;
    }

    uint8_t payload_hash[CVOTE_REGISTRATION_PAYLOAD_HASH_LENGTH] = {0};
    auxDataHashBuilder_cVoteRegistration_finalizePayload(
        &aux_data->hash_builder,
        payload_hash,
        sizeof(payload_hash));
    TRACE("CVote registration payload hash");
    TRACE_BUFFER(payload_hash, sizeof(payload_hash));

    getCVoteRegistrationSignature(&aux_data->staking_credential.keyPath,
                                  payload_hash,
                                  sizeof(payload_hash),
                                  aux_data->registration_signature,
                                  sizeof(aux_data->registration_signature));
    TRACE("CVote registration signature");
    TRACE_BUFFER(aux_data->registration_signature, sizeof(aux_data->registration_signature));

    auxDataHashBuilder_cVoteRegistration_addSignature(
        &aux_data->hash_builder,
        aux_data->registration_signature,
        sizeof(aux_data->registration_signature));
    auxDataHashBuilder_cVoteRegistration_addAuxiliaryScripts(&aux_data->hash_builder);
    return true;
}

static bool cvote_hash_builder_add_delegation(cvote_aux_data_t *aux_data,
                                             const ext_credential_t *credential,
                                             uint32_t weight) {
    LEDGER_ASSERT(aux_data != NULL, "Auxiliary data cannot be null");
    LEDGER_ASSERT(credential != NULL, "Credential cannot be null");

    uint8_t pubkey[PUBLIC_KEY_SIZE] = {0};
    if (!cvote_extract_pubkey(credential, pubkey)) {
        return false;
    }
    auxDataHashBuilder_cVoteRegistration_addDelegation(&aux_data->hash_builder,
                                                      pubkey,
                                                      sizeof(pubkey),
                                                      weight);
    return true;
}
static int cvote_send_aux_data_hash(void) {
    TRACE("Sending CVote auxiliary data hash");
    return io_send_response_pointer(G_context.tx_info.transaction.auxDataHash,
                                    AUX_DATA_HASH_LENGTH,
                                    SWO_SUCCESS);
}

static void cvote_finalize_aux_data(void) {
    cvote_aux_data_t *aux_data = G_context.tx_info.cvote_aux_data;
    LEDGER_ASSERT(aux_data != NULL, "Auxiliary data must be initialized before finalization");

    if (!cvote_hash_builder_add_common_fields(aux_data)) {
        send_swo_and_reset(SWO_WRONG_TX_INIT_APDU_DATA);
        return;
    }

    if (!cvote_append_registration_signature(aux_data)) {
        send_swo_and_reset(SWO_WRONG_TX_INIT_APDU_DATA);
        return;
    }

    auxDataHashBuilder_finalize(&aux_data->hash_builder,
                                G_context.tx_info.transaction.auxDataHash,
                                AUX_DATA_HASH_LENGTH);

    G_context.state.tx_state = TX_STATE_CHUNKS;
    TRACE("CVote AUX_DATA complete, ready for transaction chunks");
    cvote_send_aux_data_hash();
}

/**
 * Helper: Initialize transaction from P1_TX_INIT APDU
 * Validates all transaction metadata and checks security policy
 */
static void handle_tx_init_apdu(buffer_t *cdata) {
    G_context.tx_info.raw_tx = NULL;
    G_context.tx_info.raw_tx_len = 0;
    warning_bits_init(&G_context.tx_info.warning_bits);
    G_context.tx_info.planned_ui_pairs = 0;
    explicit_bzero(&G_context.tx_info.single_account_data, sizeof(single_account_data_t));
    explicit_bzero(&G_context.tx_info.pool_owner_path, sizeof(bip44_path_t));
    G_context.tx_info.pool_owner_path_present = false;

    // Read and validate options (fixed header)
    uint64_t options;
    if (!buffer_read_u64(cdata, &options, BE)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }
    bool tagCborSets = options & TX_OPTIONS_TAG_CBOR_SETS;
    options &= ~TX_OPTIONS_TAG_CBOR_SETS;
    if (options != 0) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }
    G_context.tx_info.transaction.tagCborSets = tagCborSets;

    // Read network parameters and signing mode
    if (!buffer_read_u8(cdata, &G_context.tx_info.transaction.networkId) ||
        !buffer_read_u32(cdata, &G_context.tx_info.transaction.protocolMagic, BE)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }

    // Validate network ID immediately - return specific error code
    if (!isValidNetworkId(G_context.tx_info.transaction.networkId)) {
        send_swo_and_reset(SWO_INVALID_NETWORK_ID);
        return;
    }

    // Validate mainnet protocol magic - return specific error code
    if (G_context.tx_info.transaction.networkId == MAINNET_NETWORK_ID &&
        G_context.tx_info.transaction.protocolMagic != MAINNET_PROTOCOL_MAGIC) {
        send_swo_and_reset(SWO_INVALID_PROTOCOL_MAGIC);
        return;
    }

    uint8_t txSigningMode;
    if (!buffer_read_u8(cdata, &txSigningMode)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }
    G_context.tx_info.transaction.txSigningMode = (sign_tx_signingmode_t) txSigningMode;

    // Read transaction structure counts (fields 0-1: inputs and outputs, always present)
    if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_inputs, BE) ||
        !buffer_read_u16(cdata, &G_context.tx_info.transaction.num_outputs, BE)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }

    // Field 3 (TTL) - optional
    uint8_t includeTtlByte;
    if (!buffer_read_u8(cdata, &includeTtlByte)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }
    if (!parseIncluded(includeTtlByte, &G_context.tx_info.transaction.includeTtl)) {
        send_swo_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
        return;
    }

    // Field 4 (certificates) - optional
    if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_certificates, BE)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }
    TRACE(">>>INIT: num_certificates=%u", G_context.tx_info.transaction.num_certificates);

    // Field 5 (withdrawals) - optional
    if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_withdrawals, BE)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }

    // Field 7 (auxiliary data hash) - optional
    uint8_t includeAuxDataHashByte;
    bool includeAuxDataHash = false;
    if (!buffer_read_u8(cdata, &includeAuxDataHashByte)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }
    if (!parseIncluded(includeAuxDataHashByte, &includeAuxDataHash)) {
        send_swo_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
        return;
    }
    G_context.tx_info.transaction.includeAuxDataHash = includeAuxDataHash;
    if (includeAuxDataHash) {
        uint8_t auxDataTypeByte = 0;
        if (!buffer_read_u8(cdata, &auxDataTypeByte)) {
            send_swo_and_reset(SWO_WRONG_TX_INIT_APDU_DATA);
        return;
        }

        if (auxDataTypeByte == AUX_DATA_TYPE_ARBITRARY_HASH) {
            G_context.tx_info.transaction.auxDataType = AUX_DATA_TYPE_ARBITRARY_HASH;
            if (!buffer_read_bytes(cdata,
                                   G_context.tx_info.transaction.auxDataHash,
                                   AUX_DATA_HASH_LENGTH)) {
                send_swo_and_reset(SWO_WRONG_TX_INIT_APDU_DATA);
        return;
            }
        } else if (auxDataTypeByte == AUX_DATA_TYPE_CVOTE_REGISTRATION) {
            G_context.tx_info.transaction.auxDataType = AUX_DATA_TYPE_CVOTE_REGISTRATION;
            explicit_bzero(G_context.tx_info.transaction.auxDataHash,
                           AUX_DATA_HASH_LENGTH);
        } else {
            send_swo_and_reset(SWO_WRONG_TX_INIT_APDU_DATA);
        return;
        }
    } else {
        G_context.tx_info.transaction.auxDataType = AUX_DATA_TYPE_ARBITRARY_HASH;
        explicit_bzero(G_context.tx_info.transaction.auxDataHash,
                       AUX_DATA_HASH_LENGTH);
    }

    // Field 8 (validity interval start) - optional
    uint8_t includeValidityIntervalStartByte;
    if (!buffer_read_u8(cdata, &includeValidityIntervalStartByte)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }
    if (!parseIncluded(includeValidityIntervalStartByte, &G_context.tx_info.transaction.includeValidityIntervalStart)) {
        send_swo_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
        return;
    }

    // Field 9 (mint) - optional
    if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_mint_asset_groups, BE)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }

    // Field 11 (script data hash) - optional
    uint8_t includeScriptDataHashByte;
    bool includeScriptDataHash = false;
    if (!buffer_read_u8(cdata, &includeScriptDataHashByte)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }
    if (!parseIncluded(includeScriptDataHashByte, &includeScriptDataHash)) {
        send_swo_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
        return;
    }
    G_context.tx_info.transaction.includeScriptDataHash = includeScriptDataHash;

    // Field 13 (collateral inputs)
    if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_collateral_inputs, BE)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }

    // Field 14 (required signers)
    if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_required_signers, BE)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }

    // Field 15 (network ID)
    uint8_t includeNetworkIdByte;
    if (!buffer_read_u8(cdata, &includeNetworkIdByte)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }
    if (!parseIncluded(includeNetworkIdByte, &G_context.tx_info.transaction.includeNetworkId)) {
        send_swo_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
        return;
    }

    // Field 16 (collateral output)
    uint8_t includeCollateralOutputByte;
    if (!buffer_read_u8(cdata, &includeCollateralOutputByte)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }
    if (!parseIncluded(includeCollateralOutputByte, &G_context.tx_info.transaction.includeCollateralOutput)) {
        send_swo_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
        return;
    }

    // Field 17 (total collateral)
    uint8_t includeTotalCollateralByte;
    if (!buffer_read_u8(cdata, &includeTotalCollateralByte)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }
    if (!parseIncluded(includeTotalCollateralByte, &G_context.tx_info.transaction.includeTotalCollateral)) {
        send_swo_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
        return;
    }

    // Field 18 (reference inputs)
    if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_reference_inputs, BE)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }

    // Field 19 (voting procedures)
    if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_voters, BE)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }

    // Field 21 (treasury) - optional
    uint8_t includeTreasuryByte;
    if (!buffer_read_u8(cdata, &includeTreasuryByte)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }
    if (!parseIncluded(includeTreasuryByte, &G_context.tx_info.transaction.includeTreasury)) {
        send_swo_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
        return;
    }

    // Field 22 (donation) - optional
    uint8_t includeDonationByte;
    if (!buffer_read_u8(cdata, &includeDonationByte)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }
    if (!parseIncluded(includeDonationByte, &G_context.tx_info.transaction.includeDonation)) {
        send_swo_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
        return;
    }

    // Read number of witnesses
    if (!buffer_read_u16(cdata, &G_context.tx_info.num_witnesses, BE)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }

    TRACE("TX Mode=%d, Network: ID=%d, Magic=%d, Inputs=%d, Outputs=%d, Withdrawals=%d, Mint=%d, TTL=%d, VIS=%d, Witnesses=%d",
        G_context.tx_info.transaction.txSigningMode,
        G_context.tx_info.transaction.networkId,
        G_context.tx_info.transaction.protocolMagic,
        G_context.tx_info.transaction.num_inputs,
        G_context.tx_info.transaction.num_outputs,
        G_context.tx_info.transaction.num_withdrawals,
        G_context.tx_info.transaction.num_mint_asset_groups,
        G_context.tx_info.transaction.includeTtl,
        G_context.tx_info.transaction.includeValidityIntervalStart,
        G_context.tx_info.num_witnesses
    );

    // Check security policy
    bool includeMint = (G_context.tx_info.transaction.num_mint_asset_groups > 0);

    security_policy_t init_policy = policyForSignTxInit(
        G_context.tx_info.transaction.txSigningMode,
        G_context.tx_info.transaction.networkId,
        G_context.tx_info.transaction.protocolMagic,
        G_context.tx_info.transaction.num_outputs,
        G_context.tx_info.transaction.num_certificates,
        G_context.tx_info.transaction.num_withdrawals,
        includeMint,
        G_context.tx_info.transaction.includeScriptDataHash,
        G_context.tx_info.transaction.num_collateral_inputs,
        G_context.tx_info.transaction.num_required_signers,
        G_context.tx_info.transaction.includeNetworkId,
        G_context.tx_info.transaction.includeCollateralOutput,
        G_context.tx_info.transaction.includeTotalCollateral,
        G_context.tx_info.transaction.num_reference_inputs,
        G_context.tx_info.transaction.num_voters,
        G_context.tx_info.transaction.includeTreasury,
        G_context.tx_info.transaction.includeDonation,
        &G_context.tx_info.warning_bits);

    TRACE("Transaction init security policy: %d", (int) init_policy);

    if (init_policy == POLICY_DENY) {
        TRACE("Security policy DENY - rejecting transaction init");
        send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
        return;
    }

    // Show spinner to indicate transaction data is being processed
    TRACE("Calling nbgl_useCaseSpinner(\"Processing\")");
    nbgl_useCaseSpinner("Processing");

    G_context.tx_info.cvote_aux_data_expected = includeAuxDataHash &&
                                                (G_context.tx_info.transaction.auxDataType ==
                                                 AUX_DATA_TYPE_CVOTE_REGISTRATION);
    G_context.tx_info.cvote_aux_data_initialized = false;
    G_context.tx_info.cvote_registrations_remaining = 0;

    if (G_context.tx_info.cvote_aux_data_expected) {
        G_context.state.tx_state = TX_STATE_AUX_DATA;
        TRACE("Transaction initialized, waiting for CVote AUX_DATA");
    } else {
        // Transition to CHUNKS state - now ready to receive transaction data chunks
        G_context.state.tx_state = TX_STATE_CHUNKS;
        TRACE("Transaction initialized, waiting for data chunks");
    }

    io_send_sw(SWO_SUCCESS);
}

/**
 * Helper: Accumulate transaction data chunks into buffer
 * Returns SWO_SUCCESS if more chunks expected, or falls through to parse if final chunk
 */
static void handle_tx_data_chunk(buffer_t *cdata, bool more) {
    TRACE("SWO_SUCCESS constant = 0x%04x", SWO_SUCCESS);
    // Validate we're in the correct state for receiving chunks
    if (G_context.state.tx_state != TX_STATE_CHUNKS) {
        TRACE("Invalid state for chunk reception: expected TX_STATE_CHUNKS, got %d", G_context.state.tx_state);
        send_swo_and_reset(SWO_BAD_STATE);
        return;
    }

    // Allocate buffer on first data chunk
    if (G_context.tx_info.raw_tx == NULL) {
        TRACE("Allocating transaction buffer: %d bytes", TX_BUFFER_SIZE);
        app_mem_dump_stats();
        G_context.tx_info.raw_tx = (uint8_t *) app_mem_alloc(TX_BUFFER_SIZE);
        if (G_context.tx_info.raw_tx == NULL) {
            TRACE("Failed to allocate %d byte transaction buffer!", TX_BUFFER_SIZE);
            app_mem_dump_stats();
            send_swo_and_reset(SWO_INSUFFICIENT_MEMORY);
        return;
        }
        TRACE("Transaction buffer allocated: %d bytes at %p", TX_BUFFER_SIZE, G_context.tx_info.raw_tx);
    }

    // Check if adding this chunk would exceed buffer
    if (G_context.tx_info.raw_tx_len + cdata->size > TX_BUFFER_SIZE) {
        TRACE("Transaction too large: current=%d, chunk=%d, max=%d",
              G_context.tx_info.raw_tx_len, cdata->size, TX_BUFFER_SIZE);
        send_swo_and_reset(SWO_INVALID_TX_LENGTH);
        return;
    }

    // Copy chunk data
    if (!buffer_move(cdata,
                     G_context.tx_info.raw_tx + G_context.tx_info.raw_tx_len,
                     cdata->size)) {
        TRACE("Failed to copy transaction chunk");
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }
    G_context.tx_info.raw_tx_len += cdata->size;
    TRACE("Copied %d bytes, total: %d", cdata->size, G_context.tx_info.raw_tx_len);

    if (more) {
        io_send_sw(SWO_SUCCESS);
        return;
    }

    // Final chunk - will be handled by caller
}

void handler_sign_tx(buffer_t *cdata, uint8_t p1) {
    switch (p1) {
        case P1_TX_INIT:
            G_context.req_type = REQUEST_SIGN_TRANSACTION;
            G_context.state.tx_state = TX_STATE_NONE;
            handle_tx_init_apdu(cdata);
            return;

        case P1_TX_DATA_CHUNK:
            if (G_context.req_type != REQUEST_SIGN_TRANSACTION) {
                send_swo_and_reset(SWO_BAD_STATE);
                return;
            }

            // More data chunks to follow
            handle_tx_data_chunk(cdata, true);
            return;

        case P1_TX_CHUNK_LAST:
            if (G_context.req_type != REQUEST_SIGN_TRANSACTION) {
                send_swo_and_reset(SWO_BAD_STATE);
                return;
            }

            // Final chunk
            handle_tx_data_chunk(cdata, false);

            // Parse and build hash
            LEDGER_ASSERT(G_context.state.tx_state == TX_STATE_CHUNKS, "Bad state before parse");
            G_context.state.tx_state = TX_STATE_RECEIVED;

            LEDGER_ASSERT(G_context.tx_info.raw_tx != NULL, "Raw transaction buffer missing");

            buffer_t buf = {
                .ptr = G_context.tx_info.raw_tx,
                .size = G_context.tx_info.raw_tx_len,
                .offset = 0
            };

            parser_status_e parse_status = parse_tx(&buf, &G_context.tx_info.transaction);
            if (parse_status != PARSING_OK) {
                tx_handle_parse_error(parse_status);
                return;
            }
            G_context.state.tx_state = TX_STATE_PARSED;
            tx_ui_plan_t ui_plan = {0};
            // Validate transaction and compute hash. On failure, stop immediately before UI prep.
            int validation_status = tx_validate_and_compute_hash(&ui_plan);
            if (validation_status != SWO_SUCCESS) {
                send_swo_and_reset(validation_status);
                return;
            }

            G_context.state.tx_state = TX_STATE_HASHED;

            LEDGER_ASSERT(ui_plan.pair_count > 0, "Invalid UI plan");
            G_context.tx_info.planned_ui_pairs = ui_plan.pair_count;

            int ui_prep_result = ui_prepare_transaction_review();
            if (ui_prep_result != SWO_SUCCESS) {
                tx_review_cleanup();
                send_swo_and_reset(ui_prep_result);
                return;
            }

            G_context.state.tx_state = TX_STATE_UI_PREPARED;
            ui_display_transaction();
            return;

        default:
            send_swo_and_reset(SWO_INCORRECT_P1_P2);
            return;
    }
}


void handler_sign_tx_aux_data(buffer_t *cdata, uint8_t p2) {
    if (G_context.req_type != REQUEST_SIGN_TRANSACTION) {
        send_swo_and_reset(SWO_BAD_STATE);
        return;
    }

    if (G_context.state.tx_state != TX_STATE_AUX_DATA) {
        TRACE("Bad state for AUX_DATA: expected TX_STATE_AUX_DATA, got %d", G_context.state.tx_state);
        send_swo_and_reset(SWO_BAD_STATE);
        return;
    }

    if (!G_context.tx_info.cvote_aux_data_expected) {
        TRACE("Unexpected CVote AUX_DATA APDU");
        send_swo_and_reset(SWO_BAD_STATE);
        return;
    }

    if (p2 == P2_AUX_DATA_INIT) {
        if (G_context.tx_info.cvote_aux_data_initialized) {
            send_swo_and_reset(SWO_BAD_STATE);
            return;
        }

        cvote_aux_data_t *parsed = NULL;
        cvote_parser_status_t status = cvote_parse_aux_data_init(cdata, &parsed);
        if (status != CVOTE_PARSER_OK) {
            send_swo_and_reset(status == CVOTE_PARSER_OUT_OF_MEMORY
                               ? SWO_INSUFFICIENT_MEMORY
                               : SWO_WRONG_TX_INIT_APDU_DATA);
            return;
        }

        cvote_hash_builder_setup(parsed);

        G_context.tx_info.cvote_aux_data_initialized = true;
        G_context.tx_info.cvote_aux_data = parsed;
        G_context.tx_info.cvote_registrations_remaining = parsed->delegation_count;
        TRACE("CVote AUX_DATA init: format=%u, delegations=%u", parsed->format, parsed->delegation_count);

        if (cvote_aux_data_is_done()) {
            cvote_finalize_aux_data();
            return;
        }

        io_send_sw(SWO_SUCCESS);
        return;
    }

    if (p2 == P2_AUX_DATA_DELEGATION) {
        if (!G_context.tx_info.cvote_aux_data_initialized) {
            send_swo_and_reset(SWO_BAD_STATE);
            return;
        }
        if (G_context.tx_info.cvote_registrations_remaining == 0) {
            send_swo_and_reset(SWO_BAD_STATE);
            return;
        }

        TRACE("CVote AUX_DATA delegation received, remaining=%u, payload_len=%u",
              G_context.tx_info.cvote_registrations_remaining - 1,
              cdata->size);

        cvote_aux_data_t *aux_data = G_context.tx_info.cvote_aux_data;
        ext_credential_t delegation_credential = {0};
        if (cvote_parse_credential(cdata, &delegation_credential, "Delegation credential") !=
            CVOTE_PARSER_OK) {
            send_swo_and_reset(SWO_WRONG_TX_INIT_APDU_DATA);
            return;
        }

        uint32_t weight = 0;
        if (!buffer_read_u32(cdata, &weight, BE) || cdata->offset != cdata->size) {
            send_swo_and_reset(SWO_WRONG_TX_INIT_APDU_DATA);
            return;
        }

        if (!cvote_hash_builder_add_delegation(aux_data, &delegation_credential, weight)) {
            send_swo_and_reset(SWO_WRONG_TX_INIT_APDU_DATA);
            return;
        }

        G_context.tx_info.cvote_registrations_remaining--;

        if (cvote_aux_data_is_done()) {
            cvote_finalize_aux_data();
            return;
        }

        io_send_sw(SWO_SUCCESS);
        return;
    }

    send_swo_and_reset(SWO_INCORRECT_P1_P2);
}


// All witnesses processed
void finalize_witness(bool confirm)
{
    if (!confirm) {
        // Reject entire signing operation - no more witnesses will be processed
        send_swo_and_reset(SWO_CONDITIONS_NOT_SATISFIED);
        return;
        return;
    }

    // Witness confirmed - send signature back
    io_send_response_pointer(
        G_context.tx_info.witness_signature,
        ED25519_SIGNATURE_LENGTH,
        SWO_SUCCESS
    );
    G_context.tx_info.current_witness++;
    if (G_context.tx_info.current_witness == G_context.tx_info.num_witnesses) {
        // All witnesses processed - reset context to prevent further APDUs for this tx
        reset_app_context();
    }
}

void handler_sign_tx_witness(buffer_t *cdata) {
    // Verify we're in correct state for witness signing
    if (G_context.req_type != REQUEST_SIGN_TRANSACTION) {
        TRACE("Bad request type for witness signing: %d", G_context.req_type);
        send_swo_and_reset(SWO_BAD_STATE);
        return;
    }

    if (G_context.state.tx_state != TX_STATE_APPROVED) {
        TRACE("Bad state for witness signing: expected TX_STATE_APPROVED, got %d", G_context.state.tx_state);
        send_swo_and_reset(SWO_BAD_STATE);
        return;
    }

    // Check that we haven't exceeded the expected number of witnesses
    if (G_context.tx_info.current_witness >= G_context.tx_info.num_witnesses) {
        TRACE("Witness count exceeded: current=%d, expected=%d",
              G_context.tx_info.current_witness,
              G_context.tx_info.num_witnesses
        );
        send_swo_and_reset(SWO_BAD_STATE);
        return;
    }

    // Parse witness path from APDU data
    // buffer_read_bip44_path reads the length byte and all path components
    if (!buffer_read_bip44_path(cdata, &G_context.tx_info.witness_path)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }

    TRACE("Witness %d: path length=%d",
           G_context.tx_info.current_witness,
           G_context.tx_info.witness_path.length);

    // Check security policy for witness signing
    // Determine if mint is present in the transaction
    bool mintPresent = (G_context.tx_info.transaction.num_mint_asset_groups > 0);

    // Get pool owner path if this is a pool registration
    const bip44_path_t* poolOwnerPath = NULL;
    if (G_context.tx_info.transaction.txSigningMode == SIGN_TX_SIGNINGMODE_POOL_REGISTRATION_OWNER ||
        G_context.tx_info.transaction.txSigningMode == SIGN_TX_SIGNINGMODE_POOL_REGISTRATION_OPERATOR) {
        if (G_context.tx_info.transaction.txSigningMode == SIGN_TX_SIGNINGMODE_POOL_REGISTRATION_OWNER &&
            G_context.tx_info.pool_owner_path_present) {
            poolOwnerPath = &G_context.tx_info.pool_owner_path;
        }
    }

    warning_bits_t witness_warnings = {0};
    warning_bits_init(&witness_warnings);
    security_policy_t policy = policyForSignTxWitness(
        G_context.tx_info.transaction.txSigningMode,
        &G_context.tx_info.witness_path,
        mintPresent,
        poolOwnerPath,
        &witness_warnings
    );

    TRACE("Witness security policy: %d", (int) policy);

    // Handle DENY policy
    if (policy == POLICY_DENY) {
        TRACE("Security policy DENY - rejecting witness");
        send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
        return;
    }

    // Sign the transaction hash with the witness path
    getWitness(&G_context.tx_info.witness_path,
               G_context.tx_info.tx_hash,
               sizeof(G_context.tx_info.tx_hash),
               G_context.tx_info.witness_signature,
               sizeof(G_context.tx_info.witness_signature));

    TRACE("Witness signature: %.*H", ED25519_SIGNATURE_LENGTH, G_context.tx_info.witness_signature);

    switch (policy) {
        case POLICY_HIDE:
            // POLICY_HIDE: witness does not require user confirmation
            // Finalize directly without displaying UI (similar to silent pubkey export)
            finalize_witness(true);
            return;

        case POLICY_SHOW:
            ui_display_witness(&G_context.tx_info.witness_path, policy, witness_warnings);
            return;

        case POLICY_DENY:
            // Already handled earlier in function - should never reach here
            LEDGER_ASSERT(false, "POLICY_DENY should be handled before signing");

        default:
            LEDGER_ASSERT(false, "Invalid security policy");
    }
}
