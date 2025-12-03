/*****************************************************************************
 *   Ledger App Boilerplate.
 *   (c) 2020 Ledger SAS.
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

#include "os.h"
#include "buffer.h"
#include "nbgl_use_case.h"

#include "sign_tx.h"
#include "sw.h"
#include "globals.h"
#include "display.h"
#include "tx_types.h"
#include "tx_output_types.h"
#include "tx_warnings.h"
#include "deserialize.h"
#include "mem.h"
#include "constants.h"
#include "types.h"
#include "utils/utils.h"
#include "utils/os_utils.h"
#include "utils/cbor.h"
#include "txHashBuilder/txHashBuilder.h"
#include "messageSigning.h"
#include "securityPolicy/securityPolicy.h"
#include "dispatcher.h"
#include "addressUtils/bip44.h"
#include "addressUtils/addressUtilsShelley.h"
#include "transaction/tx_utils.h"
#include "ui/menu.h"

/**
 * Helper: Initialize transaction from P1_TX_INIT APDU
 * Validates all transaction metadata and checks security policy
 */
static int handle_tx_init_apdu(buffer_t *cdata) {
    G_context.tx_info.raw_tx = NULL;
    G_context.tx_info.raw_tx_len = 0;
    G_context.tx_info.warning_list = NULL;

    // Read and validate options (fixed header)
    uint64_t options;
    if (!buffer_read_u64(cdata, &options, BE)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }
    bool tagCborSets = options & TX_OPTIONS_TAG_CBOR_SETS;
    options &= ~TX_OPTIONS_TAG_CBOR_SETS;
    if (options != 0) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }
    G_context.tx_info.transaction.tagCborSets = tagCborSets;

    // Read network parameters and signing mode
    if (!buffer_read_u8(cdata, &G_context.tx_info.transaction.networkId) ||
        !buffer_read_u32(cdata, &G_context.tx_info.transaction.protocolMagic, BE)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }

    uint8_t txSigningMode;
    if (!buffer_read_u8(cdata, &txSigningMode)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }
    G_context.tx_info.transaction.txSigningMode = (sign_tx_signingmode_t) txSigningMode;

    // Read transaction structure counts (fields 0-1: inputs and outputs, always present)
    if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_inputs, BE) ||
        !buffer_read_u16(cdata, &G_context.tx_info.transaction.num_outputs, BE)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }

    // Field 3 (TTL) - optional
    uint8_t includeTtlByte;
    if (!buffer_read_u8(cdata, &includeTtlByte)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeTtlByte, &G_context.tx_info.transaction.includeTtl)) {
        return send_error_and_reset(SW_TX_PARSING_FAIL_INCLUSION_FLAG);
    }

    // Field 4 (certificates) - optional, not implemented yet
    uint16_t num_certificates_dummy;
    if (!buffer_read_u16(cdata, &num_certificates_dummy, BE)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }

    // Field 5 (withdrawals) - optional
    if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_withdrawals, BE)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }

    // Field 7 (auxiliary data hash) - optional, not implemented yet
    uint8_t includeAuxDataHashByte;
    bool includeAuxDataHash = false;
    if (!buffer_read_u8(cdata, &includeAuxDataHashByte)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeAuxDataHashByte, &includeAuxDataHash)) {
        return send_error_and_reset(SW_TX_PARSING_FAIL_INCLUSION_FLAG);
    }
    // TODO: Store includeAuxDataHash when auxiliary data is implemented

    // Field 8 (validity interval start) - optional
    uint8_t includeValidityIntervalStartByte;
    if (!buffer_read_u8(cdata, &includeValidityIntervalStartByte)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeValidityIntervalStartByte, &G_context.tx_info.transaction.includeValidityIntervalStart)) {
        return send_error_and_reset(SW_TX_PARSING_FAIL_INCLUSION_FLAG);
    }

    // Field 9 (mint) - optional
    if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_mint_asset_groups, BE)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }

    // Field 11 (script data hash) - optional, not implemented yet
    uint8_t includeScriptDataHashByte;
    bool includeScriptDataHash = false;
    if (!buffer_read_u8(cdata, &includeScriptDataHashByte)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeScriptDataHashByte, &includeScriptDataHash)) {
        return send_error_and_reset(SW_TX_PARSING_FAIL_INCLUSION_FLAG);
    }
    // TODO: Store includeScriptDataHash when script data hash is implemented

    // Field 13 (collateral inputs) - optional, not implemented yet
    uint16_t num_collateral_inputs_dummy;
    if (!buffer_read_u16(cdata, &num_collateral_inputs_dummy, BE)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }

    // Field 14 (required signers) - optional, not implemented yet
    uint16_t num_required_signers_dummy;
    if (!buffer_read_u16(cdata, &num_required_signers_dummy, BE)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }

    // Field 15 (network ID) - optional, not implemented yet
    uint8_t includeNetworkIdByte;
    bool includeNetworkId = false;
    if (!buffer_read_u8(cdata, &includeNetworkIdByte)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeNetworkIdByte, &includeNetworkId)) {
        return send_error_and_reset(SW_TX_PARSING_FAIL_INCLUSION_FLAG);
    }
    // TODO: Store includeNetworkId when network ID is implemented

    // Field 16 (collateral output) - optional, not implemented yet
    uint8_t includeCollateralOutputByte;
    bool includeCollateralOutput = false;
    if (!buffer_read_u8(cdata, &includeCollateralOutputByte)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeCollateralOutputByte, &includeCollateralOutput)) {
        return send_error_and_reset(SW_TX_PARSING_FAIL_INCLUSION_FLAG);
    }
    // TODO: Store includeCollateralOutput when collateral output is implemented

    // Field 17 (total collateral) - optional, not implemented yet
    uint8_t includeTotalCollateralByte;
    bool includeTotalCollateral = false;
    if (!buffer_read_u8(cdata, &includeTotalCollateralByte)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeTotalCollateralByte, &includeTotalCollateral)) {
        return send_error_and_reset(SW_TX_PARSING_FAIL_INCLUSION_FLAG);
    }
    // TODO: Store includeTotalCollateral when total collateral is implemented

    // Field 18 (reference inputs) - optional, not implemented yet
    uint16_t num_reference_inputs_dummy;
    if (!buffer_read_u16(cdata, &num_reference_inputs_dummy, BE)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }

    // Field 19 (voting procedures) - optional, not implemented yet
    uint16_t num_voting_procedures_dummy;
    if (!buffer_read_u16(cdata, &num_voting_procedures_dummy, BE)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }

    // Field 21 (treasury) - optional, not implemented yet
    uint8_t includeTreasuryByte;
    bool includeTreasury = false;
    if (!buffer_read_u8(cdata, &includeTreasuryByte)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeTreasuryByte, &includeTreasury)) {
        return send_error_and_reset(SW_TX_PARSING_FAIL_INCLUSION_FLAG);
    }
    // TODO: Store includeTreasury when treasury is implemented

    // Field 22 (donation) - optional, not implemented yet
    uint8_t includeDonationByte;
    bool includeDonation = false;
    if (!buffer_read_u8(cdata, &includeDonationByte)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeDonationByte, &includeDonation)) {
        return send_error_and_reset(SW_TX_PARSING_FAIL_INCLUSION_FLAG);
    }
    // TODO: Store includeDonation when donation is implemented

    // Read number of witnesses
    if (!buffer_read_u16(cdata, &G_context.tx_info.num_witnesses, BE)) {
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
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
    security_policy_t init_policy = policyForSignTxInit(
        G_context.tx_info.transaction.txSigningMode,
        G_context.tx_info.transaction.networkId,
        G_context.tx_info.transaction.protocolMagic,
        G_context.tx_info.transaction.num_outputs,
        0,      // numCertificates - not implemented yet
        G_context.tx_info.transaction.num_withdrawals,
        false,  // includeMint - not implemented yet
        false,  // includeScriptDataHash - not implemented yet
        0,      // numCollateralInputs - not implemented yet
        0,      // numRequiredSigners - not implemented yet
        false,  // includeNetworkId - not implemented yet
        false,  // includeCollateralOutput - not implemented yet
        false,  // includeTotalCollateral - not implemented yet
        0,      // numReferenceInputs - not implemented yet
        0,      // numVotingProcedures - not implemented yet
        false,  // includeTreasury - not implemented yet
        false); // includeDonation - not implemented yet

    TRACE("Transaction init security policy: %d", (int) init_policy);

    if (init_policy == POLICY_DENY) {
        TRACE("Security policy DENY - rejecting transaction init");
        return send_error_and_reset(ERR_REJECTED_BY_POLICY);
    }

    // Show spinner to indicate transaction data is being processed
    nbgl_useCaseSpinner("Processing");

    // Transition to CHUNKS state - now ready to receive transaction data chunks
    G_context.state.tx_state = TX_STATE_CHUNKS;
    TRACE("Transaction initialized, waiting for data chunks");

    return io_send_sw(SW_OK);
}

/**
 * Helper: Accumulate transaction data chunks into buffer
 * Returns SW_OK if more chunks expected, or falls through to parse if final chunk
 */
static int handle_tx_data_chunk(buffer_t *cdata, bool more) {
    // Validate we're in the correct state for receiving chunks
    if (G_context.state.tx_state != TX_STATE_CHUNKS) {
        TRACE("Invalid state for chunk reception: expected TX_STATE_CHUNKS, got %d", G_context.state.tx_state);
        return send_error_and_reset(SW_BAD_STATE);
    }

    // Allocate buffer on first data chunk
    if (G_context.tx_info.raw_tx == NULL) {
        TRACE("Allocating transaction buffer: %d bytes", TX_BUFFER_SIZE);
        app_mem_dump_stats();
        G_context.tx_info.raw_tx = (uint8_t *) app_mem_alloc(TX_BUFFER_SIZE);
        if (G_context.tx_info.raw_tx == NULL) {
            TRACE("Failed to allocate %d byte transaction buffer!", TX_BUFFER_SIZE);
            app_mem_dump_stats();
            return send_error_and_reset(SW_INSUFFICIENT_MEMORY);
        }
        TRACE("Transaction buffer allocated: %d bytes at %p", TX_BUFFER_SIZE, G_context.tx_info.raw_tx);
    }

    // Check if adding this chunk would exceed buffer
    if (G_context.tx_info.raw_tx_len + cdata->size > TX_BUFFER_SIZE) {
        TRACE("Transaction too large: current=%d, chunk=%d, max=%d",
              G_context.tx_info.raw_tx_len, cdata->size, TX_BUFFER_SIZE);
        return send_error_and_reset(SW_WRONG_TX_LENGTH);
    }

    // Copy chunk data
    if (!buffer_move(cdata,
                     G_context.tx_info.raw_tx + G_context.tx_info.raw_tx_len,
                     cdata->size)) {
        TRACE("Failed to copy transaction chunk");
        return send_error_and_reset(SW_TX_PARSING_FAIL);
    }
    G_context.tx_info.raw_tx_len += cdata->size;
    TRACE("Copied %d bytes, total: %d", cdata->size, G_context.tx_info.raw_tx_len);

    if (more) {
        return io_send_sw(SW_OK);
    }

    // Final chunk - will be handled by caller
    return SW_OK;
}

/**
 * Helper: Parse transaction, build hash, and prepare for UI display
 * Handles all cleanup on parse errors
 */
static int parse_and_hash_transaction(void) {
    buffer_t buf = {.ptr = G_context.tx_info.raw_tx,
                    .size = G_context.tx_info.raw_tx_len,
                    .offset = 0};

    parser_status_e status = transaction_deserialize(&buf, &G_context.tx_info.transaction);
    TRACE("Parsing status: %d", status);
    if (status != PARSING_OK) {
        tx_context_cleanup();

        // Return appropriate error based on parse failure type
        switch (status) {
            case INPUTS_PARSING_ERROR:
            case INPUTS_COUNT_PARSING_ERROR:
                return send_error_and_reset(SW_TX_PARSING_FAIL_INPUTS);
            case OUTPUTS_PARSING_ERROR:
            case OUTPUTS_COUNT_PARSING_ERROR:
            case OUTPUT_DESTINATION_TYPE_ERROR:
            case OUTPUT_ADDRESS_SIZE_ERROR:
                return send_error_and_reset(SW_TX_PARSING_FAIL_OUTPUTS);
            case FEE_PARSING_ERROR:
                return send_error_and_reset(SW_TX_PARSING_FAIL_FEE);
            case TO_PARSING_ERROR:
                return send_error_and_reset(SW_TX_PARSING_FAIL_TTL);
            default:
                return send_error_and_reset(SW_TX_PARSING_FAIL);
        }
    }

    // Transition from CHUNKS to PARSED state
    G_context.state.tx_state = TX_STATE_PARSED;

    // Fill in Byron protocol magic for DEVICE_OWNED outputs
    s_flist_node *output_node = G_context.tx_info.transaction.outputs;
    while (output_node != NULL) {
        tx_output_list_item_t *output_item = (tx_output_list_item_t *) output_node;
        if (output_item->output_data.destination.type == DESTINATION_DEVICE_OWNED &&
            output_item->output_data.destination.params.type == BYRON) {
            output_item->output_data.destination.params.protocolMagic =
                G_context.tx_info.transaction.protocolMagic;
        }
        output_node = output_node->next;
    }

    // Check for high fee warning
    if (G_context.tx_info.transaction.fee > HIGH_FEE_WARNING_THRESHOLD) {
        TRACE("High fee detected: %llu lovelace (threshold: %u lovelace)",
              G_context.tx_info.transaction.fee, HIGH_FEE_WARNING_THRESHOLD);
        if (!tx_warning_add((tx_warning_list_item_t **)&G_context.tx_info.warning_list,
                           TX_WARNING_HIGH_FEE,
                           G_context.tx_info.transaction.networkId,
                           G_context.tx_info.transaction.protocolMagic)) {
            TRACE("Warning allocation failed");
            tx_context_cleanup();
            return send_error_and_reset(SW_INSUFFICIENT_MEMORY);
        }
    }

    // Build transaction hash
    tx_hash_builder_t txHashBuilder;
    explicit_bzero(&txHashBuilder, sizeof(txHashBuilder));

    txHashBuilder_init(&txHashBuilder,
                      G_context.tx_info.transaction.tagCborSets,
                      G_context.tx_info.transaction.num_inputs,
                      G_context.tx_info.transaction.num_outputs,
                      G_context.tx_info.transaction.includeTtl,
                      0,      // numCertificates
                      G_context.tx_info.transaction.num_withdrawals,
                      false,  // includeAuxData
                      G_context.tx_info.transaction.includeValidityIntervalStart,
                      G_context.tx_info.transaction.num_mint_asset_groups > 0,  // includeMint
                      false,  // includeScriptDataHash
                      0,      // numCollateralInputs
                      0,      // numRequiredSigners
                      false,  // includeNetworkId
                      false,  // includeCollateralOutput
                      false,  // includeTotalCollateral
                      0,      // numReferenceInputs
                      0,      // numVotingProcedures
                      false,  // includeTreasury
                      false); // includeDonation

    // Add inputs
    txHashBuilder_enterInputs(&txHashBuilder);
    s_flist_node *input_node = G_context.tx_info.transaction.inputs;
    while (input_node != NULL) {
        tx_input_list_item_t *item = (tx_input_list_item_t *) input_node;
        txHashBuilder_addInput(&txHashBuilder, (const tx_input_t*)&item->input_data);
        input_node = input_node->next;
    }

    // Add outputs
    txHashBuilder_enterOutputs(&txHashBuilder);
    output_node = G_context.tx_info.transaction.outputs;
    while (output_node != NULL) {
        tx_output_list_item_t *output_item = (tx_output_list_item_t *) output_node;

        tx_output_description_t output_desc;
        output_desc.format = output_item->output_data.format;
        output_desc.amount = output_item->output_data.adaAmount;
        output_desc.numAssetGroups = output_item->output_data.numAssetGroups;
        output_desc.includeDatum = (output_item->output_data.datum.type != 0xFF);
        output_desc.includeRefScript = output_item->output_data.hasRefScript;

        if (output_item->output_data.destination.type == DESTINATION_THIRD_PARTY) {
            output_desc.destination.type = DESTINATION_THIRD_PARTY;
            output_desc.destination.address.buffer = output_item->output_data.destination.address.buffer;
            output_desc.destination.address.size = output_item->output_data.destination.address.size;
            txHashBuilder_addOutput_topLevelData(&txHashBuilder, &output_desc);
        } else if (output_item->output_data.destination.type == DESTINATION_DEVICE_OWNED) {
            uint8_t *address_bytes = (uint8_t *) app_mem_alloc(MAX_ADDRESS_SIZE);
            if (address_bytes == NULL) {
                return send_error_and_reset(SW_TX_PARSING_FAIL);
            }

            TRACE("Deriving device-owned address: type=%d, paymentPath.len=%u, staking=%d",
                  output_item->output_data.destination.params.type,
                  output_item->output_data.destination.params.paymentKeyPath.length,
                  output_item->output_data.destination.params.stakingDataSource);

            size_t address_size = deriveAddress(
                &output_item->output_data.destination.params,
                address_bytes,
                MAX_ADDRESS_SIZE
            );

            TRACE("Derived address size=%u: %.*H", address_size, address_size, address_bytes);

            if (address_size == 0 || address_size > MAX_ADDRESS_SIZE) {
                app_mem_free(address_bytes);
                return send_error_and_reset(SW_TX_PARSING_FAIL);
            }

            // After derivation, treat the result as a third-party address for CBOR hashing
            output_desc.destination.type = DESTINATION_THIRD_PARTY;
            output_desc.destination.address.buffer = address_bytes;
            output_desc.destination.address.size = address_size;
            txHashBuilder_addOutput_topLevelData(&txHashBuilder, &output_desc);
            app_mem_free(address_bytes);
        }

        // Add asset groups and tokens for this output
        for (uint16_t ag = 0; ag < output_item->output_data.numAssetGroups; ag++) {
            asset_group_t *group = &output_item->output_data.assetGroups[ag];
            txHashBuilder_addOutput_tokenGroup(&txHashBuilder,
                                               group->policyId,
                                               MINTING_POLICY_ID_SIZE,
                                               group->numTokens);

            for (uint16_t tk = 0; tk < group->numTokens; tk++) {
                output_token_t *token = &group->tokens[tk];
                txHashBuilder_addOutput_token(&txHashBuilder,
                                              token->assetName,
                                              token->assetNameLen,
                                              (uint64_t)token->amount);
            }
        }

        output_node = output_node->next;
    }

    // Add fee
    txHashBuilder_addFee(&txHashBuilder, G_context.tx_info.transaction.fee);

    // Add TTL if included
    if (G_context.tx_info.transaction.includeTtl) {
        txHashBuilder_addTtl(&txHashBuilder, G_context.tx_info.transaction.ttl);
    }

    // Add withdrawals if present
    if (G_context.tx_info.transaction.num_withdrawals > 0) {
        txHashBuilder_enterWithdrawals(&txHashBuilder);
        s_flist_node *withdrawal_node = G_context.tx_info.transaction.withdrawals;
        while (withdrawal_node != NULL) {
            tx_withdrawal_list_item_t *withdrawal_item = (tx_withdrawal_list_item_t *) withdrawal_node;

            // Construct reward address from withdrawal credential
            uint8_t reward_address[REWARD_ACCOUNT_SIZE];
            size_t reward_addr_len = 0;

            switch (withdrawal_item->withdrawal_data.stakeCredential.type) {
                case EXT_CREDENTIAL_KEY_PATH: {
                    reward_addr_len = constructRewardAddressFromKeyPath(
                        &withdrawal_item->withdrawal_data.stakeCredential.keyPath,
                        G_context.tx_info.transaction.networkId,
                        reward_address,
                        sizeof(reward_address)
                    );
                    break;
                }
                case EXT_CREDENTIAL_KEY_HASH: {
                    reward_addr_len = constructRewardAddressFromHash(
                        G_context.tx_info.transaction.networkId,
                        REWARD_HASH_SOURCE_KEY,
                        withdrawal_item->withdrawal_data.stakeCredential.keyHash,
                        ADDRESS_KEY_HASH_LENGTH,
                        reward_address,
                        sizeof(reward_address)
                    );
                    break;
                }
                case EXT_CREDENTIAL_SCRIPT_HASH: {
                    reward_addr_len = constructRewardAddressFromHash(
                        G_context.tx_info.transaction.networkId,
                        REWARD_HASH_SOURCE_SCRIPT,
                        withdrawal_item->withdrawal_data.stakeCredential.scriptHash,
                        SCRIPT_HASH_LENGTH,
                        reward_address,
                        sizeof(reward_address)
                    );
                    break;
                }
                default:
                    return send_error_and_reset(SW_TX_PARSING_FAIL);
            }

            if (reward_addr_len == 0 || reward_addr_len != REWARD_ACCOUNT_SIZE) {
                return send_error_and_reset(SW_TX_PARSING_FAIL);
            }

            // Add withdrawal to hash builder
            txHashBuilder_addWithdrawal(&txHashBuilder,
                                       reward_address,
                                       reward_addr_len,
                                       withdrawal_item->withdrawal_data.amount);

            withdrawal_node = withdrawal_node->next;
        }
    }

    // Add validity interval start if included
    if (G_context.tx_info.transaction.includeValidityIntervalStart) {
        txHashBuilder_addValidityIntervalStart(&txHashBuilder,
                                               G_context.tx_info.transaction.validityIntervalStart);
    }

    // Add mint if present
    if (G_context.tx_info.transaction.num_mint_asset_groups > 0) {
        txHashBuilder_enterMint(&txHashBuilder);
        txHashBuilder_addMint_topLevelData(&txHashBuilder,
                                           G_context.tx_info.transaction.num_mint_asset_groups);

        s_flist_node *mint_node = G_context.tx_info.transaction.mint_asset_groups;
        while (mint_node != NULL) {
            mint_asset_group_list_item_t *mint_item = (mint_asset_group_list_item_t *) mint_node;

            txHashBuilder_addMint_tokenGroup(&txHashBuilder,
                                             mint_item->asset_group.policyId,
                                             MINTING_POLICY_ID_SIZE,
                                             mint_item->asset_group.numTokens);

            for (uint16_t tk = 0; tk < mint_item->asset_group.numTokens; tk++) {
                mint_token_t *token = &mint_item->asset_group.tokens[tk];
                txHashBuilder_addMint_token(&txHashBuilder,
                                            token->assetName,
                                            token->assetNameLen,
                                            (uint64_t)token->amount);
            }

            mint_node = mint_node->next;
        }
    }

    // Finalize hash
    txHashBuilder_finalize(&txHashBuilder,
                          G_context.tx_info.tx_hash,
                          sizeof(G_context.tx_info.tx_hash));

    TRACE("Hash: %.*H", sizeof(G_context.tx_info.tx_hash), G_context.tx_info.tx_hash);

    return SW_OK;
}

int handler_sign_tx(buffer_t *cdata, uint8_t chunk_type, bool more) {
    if (chunk_type == P1_TX_INIT) {
        explicit_bzero(&G_context, sizeof(G_context));
        G_context.req_type = REQUEST_SIGN_TRANSACTION;
        G_context.state.tx_state = TX_STATE_NONE;
        return handle_tx_init_apdu(cdata);

    } else {  // parse transaction data chunks
        if (G_context.req_type != REQUEST_SIGN_TRANSACTION) {
            return send_error_and_reset(SW_BAD_STATE);
        }

        // Handle chunk accumulation
        int result = handle_tx_data_chunk(cdata, more);
        if (more || result != SW_OK) {
            return result;
        }

        // Final chunk - parse and build hash
        int parse_result = parse_and_hash_transaction();
        if (parse_result != SW_OK) {
            return parse_result;
        }

        // Display transaction for user confirmation
        return ui_display_transaction();
    }
}

// All witnesses processed
void finalize_witness()
{
    // Witness confirmed - send signature back
    io_send_response_pointer(
        G_context.tx_info.witness_signature,
        ED25519_SIGNATURE_LENGTH,
        SW_OK
    );
    G_context.tx_info.current_witness++;
    if (G_context.tx_info.current_witness == G_context.tx_info.num_witnesses) {
        tx_context_cleanup();
        G_context.req_type = REQUEST_NONE;
        G_context.state.tx_state = TX_STATE_NONE;
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, ui_menu_main);
    } else {
        nbgl_useCaseSpinner("Processing");
    }
}

int handler_sign_tx_witness(buffer_t *cdata) {
    // Verify we're in correct state for witness signing
    if (G_context.req_type != REQUEST_SIGN_TRANSACTION) {
        TRACE("Bad request type for witness signing: %d", G_context.req_type);
        return send_error_and_reset(SW_BAD_STATE);
    }

    if (G_context.state.tx_state != TX_STATE_APPROVED) {
        TRACE("Bad state for witness signing: expected TX_STATE_APPROVED, got %d", G_context.state.tx_state);
        tx_context_cleanup();
        return send_error_and_reset(SW_BAD_STATE);
    }

    // Check that we haven't exceeded the expected number of witnesses
    if (G_context.tx_info.current_witness >= G_context.tx_info.num_witnesses) {
        TRACE("Witness count exceeded: current=%d, expected=%d",
              G_context.tx_info.current_witness,
              G_context.tx_info.num_witnesses
        );
        tx_context_cleanup();
        return send_error_and_reset(SW_BAD_STATE);
    }

    // Parse witness path from APDU data
    // buffer_read_bip44_path reads the length byte and all path components
    if (!buffer_read_bip44_path(cdata, &G_context.tx_info.witness_path)) {
        tx_context_cleanup();
        return send_error_and_reset(SW_WRONG_DATA_LENGTH);
    }

    TRACE("Witness %d: path length=%d",
           G_context.tx_info.current_witness,
           G_context.tx_info.witness_path.length);

    // Check security policy for witness signing
    // Determine if mint is present in the transaction
    bool mintPresent = false; // TODO mint not implemented yet

    // Get pool owner path if this is a pool registration
    const bip44_path_t* poolOwnerPath = NULL;
    if (G_context.tx_info.transaction.txSigningMode == SIGN_TX_SIGNINGMODE_POOL_REGISTRATION_OWNER ||
        G_context.tx_info.transaction.txSigningMode == SIGN_TX_SIGNINGMODE_POOL_REGISTRATION_OPERATOR) {
        // Extract pool owner path from pool registration certificate if available
        // TODO For now, we'll pass NULL and let the policy handle it
        poolOwnerPath = NULL;
    }

    security_policy_t policy = policyForSignTxWitness(
        G_context.tx_info.transaction.txSigningMode,
        &G_context.tx_info.witness_path,
        mintPresent,
        poolOwnerPath
    );

    TRACE("Witness security policy: %d", (int) policy);

    // Handle DENY policy
    if (policy == POLICY_DENY) {
        TRACE("Security policy DENY - rejecting witness");
        tx_context_cleanup();
        return send_error_and_reset(ERR_REJECTED_BY_POLICY);
    }

    // Sign the transaction hash with the witness path
    getWitness(&G_context.tx_info.witness_path,
               G_context.tx_info.tx_hash,
               sizeof(G_context.tx_info.tx_hash),
               G_context.tx_info.witness_signature,
               sizeof(G_context.tx_info.witness_signature));

    TRACE("Witness signature: %.*H", ED25519_SIGNATURE_LENGTH, G_context.tx_info.witness_signature);

    // Handle witness based on security policy
    switch (policy) {
        case POLICY_SHOW_BEFORE_RESPONSE:
        case POLICY_PROMPT_BEFORE_RESPONSE:
        case POLICY_PROMPT_WARN_UNUSUAL:
            // Display witness path and request user confirmation
            return ui_display_witness(&G_context.tx_info.witness_path, policy);

        case POLICY_ALLOW_WITHOUT_PROMPT:
            finalize_witness();
            return 0;

        default:
            ASSERT(false);
            tx_context_cleanup();
            return send_error_and_reset(SW_BAD_STATE);
    }
}
