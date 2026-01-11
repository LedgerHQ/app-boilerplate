#include <stddef.h>  // NULL
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "tx_validate.h"

#include "os.h"
#include "app_tokens/app_tokens.h"
#include "cardano_swo.h"
#include "cardano_settings.h"
#include "globals.h"
#include "addressUtils/bip44.h"
#include "addressUtils/addressUtilsShelley.h"
#include "tx_output_types.h"
#include "transaction/tx_aux_data_types.h"
#include "transaction/tx_hash_builder.h"
#include "transaction/tx_utils.h"
#include "memory/mem.h"
#include "securityPolicy/securityPolicy.h"
#include "securityPolicy/securityWarnings.h"
#include "utils/assert.h"
#include "io.h"
#include "app_context.h"
#include "utils/cbor.h"

#define UI_PAIR_LIMIT 250

/**
 * Convert ext_credential_t to a version suitable for tx hash building.
 * Converts KEY_PATH to KEY_HASH, leaves KEY_HASH and SCRIPT_HASH unchanged.
 * Returns a credential that can be passed to txHashBuilder functions.
 * Does NOT modify the input credential (needed for security policies and UI).
 */
static ext_credential_t _credentialForTxHash(const ext_credential_t* credential) {
    ext_credential_t result = *credential;

    if (credential->type == EXT_CREDENTIAL_KEY_PATH) {
        result.type = EXT_CREDENTIAL_KEY_HASH;
        bip44_pathToKeyHash(&credential->keyPath, result.keyHash, sizeof(result.keyHash));
    }

    return result;
}

/**
 * Convert ext_drep_t to a version suitable for tx hash building.
 * Converts KEY_PATH to KEY_HASH, leaves other types unchanged.
 * Does NOT modify the input drep.
 */
static ext_drep_t _drepForTxHash(const ext_drep_t* drep) {
    ext_drep_t result = *drep;

    if (drep->type == EXT_DREP_KEY_PATH) {
        result.type = EXT_DREP_KEY_HASH;
        bip44_pathToKeyHash(&drep->keyPath, result.keyHash, sizeof(result.keyHash));
    }

    return result;
}

static ext_voter_t _voterForTxHash(const ext_voter_t* voter) {
    ext_voter_t result = *voter;

    // Convert KEY_PATH variants to KEY_HASH
    switch (voter->type) {
        case EXT_VOTER_COMMITTEE_HOT_KEY_PATH:
            result.type = EXT_VOTER_COMMITTEE_HOT_KEY_HASH;
            bip44_pathToKeyHash(&voter->keyPath, result.keyHash, sizeof(result.keyHash));
            break;
        case EXT_VOTER_DREP_KEY_PATH:
            result.type = EXT_VOTER_DREP_KEY_HASH;
            bip44_pathToKeyHash(&voter->keyPath, result.keyHash, sizeof(result.keyHash));
            break;
        case EXT_VOTER_STAKE_POOL_KEY_PATH:
            result.type = EXT_VOTER_STAKE_POOL_KEY_HASH;
            bip44_pathToKeyHash(&voter->keyPath, result.keyHash, sizeof(result.keyHash));
            break;
        // KEY_HASH and SCRIPT_HASH types: no conversion needed, just copy
        case EXT_VOTER_COMMITTEE_HOT_KEY_HASH:
        case EXT_VOTER_DREP_KEY_HASH:
        case EXT_VOTER_STAKE_POOL_KEY_HASH:
        case EXT_VOTER_COMMITTEE_HOT_SCRIPT_HASH:
        case EXT_VOTER_DREP_SCRIPT_HASH:
            // Already copied above
            break;
        default:
            ASSERT(false);
    }

    return result;
}

int tx_validate_and_compute_hash(tx_ui_plan_t* plan) {
    LEDGER_ASSERT(G_context.state.tx_state == TX_STATE_PARSED, "Validation invoked at wrong state");
    LEDGER_ASSERT(plan != NULL, "NULL plan");

    TRACE("Expert mode: %d", is_expert_mode());

    G_context.tx_info.pool_owner_path_present = false;
    plan->pair_count = 2;  // fee + tx hash
    plan->has_excessive_length_element = false;  // TODO: Implement detection during validation
    s_flist_node *input_node = G_context.tx_info.transaction.inputs;
    while (input_node != NULL) {
        tx_input_node_t *input_item = (tx_input_node_t *) input_node;
        security_policy_t input_policy = policyForSignTxInput(G_context.tx_info.transaction.txSigningMode, &input_item->input);
        LEDGER_ASSERT(input_policy != POLICY_DENY, "Input denied during UI");
        if (input_policy == POLICY_SHOW) {
            plan->pair_count++;
        }
        input_node = input_node->next;
    }
    if (G_context.tx_info.transaction.includeTtl) {
        security_policy_t ttl_policy = policyForSignTxTtl(G_context.tx_info.transaction.ttl);
        switch (ttl_policy) {
            case POLICY_DENY:
                return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
            case POLICY_SHOW:
                plan->pair_count++;
                break;
            case POLICY_HIDE:
                break;
        }
    }
    if (G_context.tx_info.transaction.includeValidityIntervalStart) {
        security_policy_t validity_interval_start_policy = policyForSignTxValidityIntervalStart();
        switch (validity_interval_start_policy) {
            case POLICY_DENY:
                return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
            case POLICY_SHOW:
                plan->pair_count++;
                break;
            case POLICY_HIDE:
                break;
        }
    }

    if (G_context.tx_info.transaction.includeAuxDataHash) {
        security_policy_t aux_policy =
            policyForSignTxAuxData(G_context.tx_info.transaction.auxDataType);
        switch (aux_policy) {
            case POLICY_DENY:
                return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
            case POLICY_SHOW:
                plan->pair_count++;
                break;
            case POLICY_HIDE:
                break;
        }
    }

    if (G_context.tx_info.transaction.num_mint_asset_groups > 0) {
        security_policy_t mint_policy =
            policyForSignTxMintInit(G_context.tx_info.transaction.txSigningMode);
        switch (mint_policy) {
            case POLICY_DENY:
                return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
            case POLICY_SHOW: {
                // summary entry
                plan->pair_count++;
                // each minted token contributes fingerprint + amount
                s_flist_node* mint_node = G_context.tx_info.transaction.mint_asset_groups;
                while (mint_node != NULL) {
                    mint_asset_group_node_t* mint_item =
                        (mint_asset_group_node_t*) mint_node;
                    if (mint_item->asset_group.tokens != NULL) {
                        plan->pair_count += (uint16_t)(2 * mint_item->asset_group.numTokens);
                    }
                    mint_node = mint_node->next;
                }
                break;
            }
            case POLICY_HIDE:
                break;
        }
    }

    tx_hash_builder_t txHashBuilder;
    explicit_bzero(&txHashBuilder, sizeof(txHashBuilder));

    txHashBuilder_init(&txHashBuilder,
                      G_context.tx_info.transaction.tagCborSets,
                      G_context.tx_info.transaction.num_inputs,
                      G_context.tx_info.transaction.num_outputs,
                      G_context.tx_info.transaction.includeTtl,
                      G_context.tx_info.transaction.num_certificates,
                      G_context.tx_info.transaction.num_withdrawals,
                      G_context.tx_info.transaction.includeAuxDataHash,
                      G_context.tx_info.transaction.includeValidityIntervalStart,
                      G_context.tx_info.transaction.num_mint_asset_groups > 0,
                      G_context.tx_info.transaction.includeScriptDataHash,
                      G_context.tx_info.transaction.num_collateral_inputs,
                      G_context.tx_info.transaction.num_required_signers,
                      G_context.tx_info.transaction.includeNetworkId,
                      G_context.tx_info.transaction.includeCollateralOutput,
                      G_context.tx_info.transaction.includeTotalCollateral,
                      G_context.tx_info.transaction.num_reference_inputs,
                      G_context.tx_info.transaction.num_voters,
                      G_context.tx_info.transaction.includeTreasury,
                      G_context.tx_info.transaction.includeDonation);

    txHashBuilder_enterInputs(&txHashBuilder);
    s_flist_node *hash_input_node = G_context.tx_info.transaction.inputs;
    while (hash_input_node != NULL) {
        tx_input_node_t *item = (tx_input_node_t *) hash_input_node;
        txHashBuilder_addInput(&txHashBuilder, (const tx_input_t*)&item->input);
        hash_input_node = hash_input_node->next;
    }

    txHashBuilder_enterOutputs(&txHashBuilder);
    s_flist_node *output_node = G_context.tx_info.transaction.outputs;
    while (output_node != NULL) {
        tx_output_node_t *output_item = (tx_output_node_t *) output_node;

        tx_output_description_t output_desc = {0};
        output_desc.format = output_item->output_data.format;
        output_desc.amount = output_item->output_data.adaAmount;
        output_desc.numAssetGroups = output_item->output_data.numAssetGroups;
        output_desc.includeDatum = output_item->output_data.datum.hasDatum;
        output_desc.includeRefScript = output_item->output_data.refScript.hasRefScript;

        if (output_item->output_data.destination.type == DESTINATION_THIRD_PARTY) {
            output_desc.destination.type = DESTINATION_THIRD_PARTY;
            output_desc.destination.address.buffer = output_item->output_data.destination.address.buffer;
            output_desc.destination.address.size = output_item->output_data.destination.address.size;
        } else {
            output_desc.destination.type = DESTINATION_DEVICE_OWNED;
            output_desc.destination.params = &output_item->output_data.destination.params;
        }

        security_policy_t datum_policy;
        security_policy_t ref_script_policy;
        security_policy_t output_policy;
        if (output_item->output_data.destination.type == DESTINATION_THIRD_PARTY) {
            output_policy = policyForSignTxOutputAddressBytes(
                &output_desc,
                G_context.tx_info.transaction.txSigningMode,
                G_context.tx_info.transaction.networkId,
                G_context.tx_info.transaction.protocolMagic,
                &G_context.tx_info.warning_bits
            );
        } else {
            output_policy = policyForSignTxOutputAddressParams(
                &output_desc,
                G_context.tx_info.transaction.txSigningMode,
                G_context.tx_info.transaction.networkId,
                G_context.tx_info.transaction.protocolMagic,
                &G_context.tx_info.warning_bits
            );
        }
        switch (output_policy) {
            case POLICY_DENY:
                TRACE("Output security policy denied");
                return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
            case POLICY_SHOW: {
                datum_policy = policyForSignTxOutputDatumHash(output_policy);
                ref_script_policy = policyForSignTxOutputRefScript(output_policy);

                // Count pairs for output: output number, address, amount
                plan->pair_count += 3;
                // For device-owned addresses, add 2 more pairs (payment info + staking info)
                if (output_item->output_data.destination.type == DESTINATION_DEVICE_OWNED) {
                    plan->pair_count += 2;
                }
                if (datum_policy == POLICY_SHOW && output_item->output_data.datum.hasDatum) {
                    plan->pair_count++;
                }
                if (ref_script_policy == POLICY_SHOW && output_item->output_data.refScript.hasRefScript) {
                    plan->pair_count++;
                }

                // Count pairs for tokens (2 pairs per token: fingerprint + amount)
                if (output_item->output_data.assetGroups != NULL) {
                    uint16_t asset_group_count = 0;
                    s_flist_node *asset_group_node = output_item->output_data.assetGroups;
                    while (asset_group_node != NULL) {
                        output_asset_group_node_t *group_node =
                            (output_asset_group_node_t *) asset_group_node;
                        output_asset_group_t *group = &group_node->asset_group;
                        // Count tokens in linked list
                        s_flist_node *token_node = group->tokens;
                        while (token_node != NULL) {
                            plan->pair_count += 2;  // fingerprint + amount per token
                            token_node = token_node->next;
                        }
                        asset_group_count++;
                        asset_group_node = asset_group_node->next;
                    }
                    LEDGER_ASSERT(asset_group_count == output_item->output_data.numAssetGroups,
                                  "Output asset group count mismatch");
                }
                break;
            }
            case POLICY_HIDE:
                datum_policy = policyForSignTxOutputDatumHash(output_policy);
                ref_script_policy = policyForSignTxOutputRefScript(output_policy);
                break;
        }

        if (output_item->output_data.destination.type == DESTINATION_THIRD_PARTY) {
            txHashBuilder_addOutput_topLevelData(&txHashBuilder, &output_desc);
        } else {
            uint8_t *address_bytes = (uint8_t *) app_mem_alloc(MAX_ADDRESS_LENGTH);
            if (address_bytes == NULL) {
                return send_swo_and_reset(SWO_INSUFFICIENT_MEMORY);
            }

            size_t address_size = deriveAddress(
                &output_item->output_data.destination.params,
                address_bytes,
                MAX_ADDRESS_LENGTH
            );

            if (address_size == 0 || address_size > MAX_ADDRESS_LENGTH) {
                app_mem_free(address_bytes);
                return send_swo_and_reset(SWO_INCORRECT_DATA);
            }

            output_desc.destination.type = DESTINATION_THIRD_PARTY;
            output_desc.destination.address.buffer = address_bytes;
            output_desc.destination.address.size = address_size;
            txHashBuilder_addOutput_topLevelData(&txHashBuilder, &output_desc);
            app_mem_free(address_bytes);
        }

        uint16_t asset_group_count = 0;
        s_flist_node *asset_group_node = output_item->output_data.assetGroups;
        while (asset_group_node != NULL) {
            output_asset_group_node_t *group_node =
                (output_asset_group_node_t *) asset_group_node;
            output_asset_group_t *group = &group_node->asset_group;
            txHashBuilder_addOutput_tokenGroup(&txHashBuilder,
                                               group->policyId,
                                               MINTING_POLICY_ID_LENGTH,
                                               group->numTokens);

            // Iterate through linked list of tokens instead of array
            s_flist_node *token_node = group->tokens;
            while (token_node != NULL) {
                output_token_node_t *token_item = (output_token_node_t *) token_node;
                output_token_t *token = &token_item->token_data;
                txHashBuilder_addOutput_token(&txHashBuilder,
                                              token->assetName,
                                              token->assetNameLen,
                                              (uint64_t)token->amount);
                token_node = token_node->next;
            }
            asset_group_count++;
            asset_group_node = asset_group_node->next;
        }
        LEDGER_ASSERT(asset_group_count == output_item->output_data.numAssetGroups,
                      "Output asset group count mismatch");

        // Add datum if present
        if (output_item->output_data.datum.hasDatum) {
            if (output_item->output_data.datum.type == DATUM_HASH) {
                txHashBuilder_addOutput_datum(&txHashBuilder,
                                             DATUM_HASH,
                                             output_item->output_data.datum.hash,
                                             OUTPUT_DATUM_HASH_LENGTH);
            } else {  // DATUM_INLINE
                txHashBuilder_addOutput_datum(&txHashBuilder,
                                             DATUM_INLINE,
                                             output_item->output_data.datum.inline_data.data,
                                             output_item->output_data.datum.inline_data.size);
                txHashBuilder_addOutput_datum_inline_chunk(&txHashBuilder,
                                                           output_item->output_data.datum.inline_data.data,
                                                           output_item->output_data.datum.inline_data.size);
            }
        }

        // Add reference script if present
        if (output_item->output_data.refScript.hasRefScript) {
            txHashBuilder_addOutput_referenceScript(&txHashBuilder,
                                                   output_item->output_data.refScript.size);
            txHashBuilder_addOutput_referenceScript_dataChunk(&txHashBuilder,
                                                             output_item->output_data.refScript.data,
                                                             output_item->output_data.refScript.size);
        }

        output_node = output_node->next;
    }

    txHashBuilder_addFee(&txHashBuilder, G_context.tx_info.transaction.fee);
    security_policy_t fee_policy = policyForSignTxFee(G_context.tx_info.transaction.txSigningMode,
                                                      G_context.tx_info.transaction.fee,
                                                      &G_context.tx_info.warning_bits);
    switch (fee_policy) {
        case POLICY_DENY:
            return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
        case POLICY_SHOW:
        case POLICY_HIDE:
            break;
    }
    if (G_context.tx_info.transaction.includeTtl) {
        txHashBuilder_addTtl(&txHashBuilder, G_context.tx_info.transaction.ttl);
    }

    if (G_context.tx_info.transaction.num_certificates > 0) {
        // Initialize certificate state in hash builder
        txHashBuilder_enterCertificates(&txHashBuilder);

        s_flist_node *certificate_node = G_context.tx_info.transaction.certificates;
        while (certificate_node != NULL) {
            tx_certificate_node_t *certificate_item =
                (tx_certificate_node_t *) certificate_node;

            // First check generic policy (only for DENY - validates certificate type is allowed in this signing mode)
            security_policy_t generic_policy = policyForSignTxCertificate(
                G_context.tx_info.transaction.txSigningMode,
                certificate_item->certificate.type
            );
            switch (generic_policy) {
                case POLICY_DENY:
                    return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                case POLICY_SHOW:
                case POLICY_HIDE:
                    break;  // Continue to type-specific policy check
            }

            // Then check type-specific policy for SHOW/HIDE decision and UI pair counting
            security_policy_t cert_policy = POLICY_HIDE;

            switch (certificate_item->certificate.type) {
                case CERTIFICATE_STAKE_REGISTRATION:
                case CERTIFICATE_STAKE_DEREGISTRATION:
                case CERTIFICATE_STAKE_REGISTRATION_CONWAY:
                case CERTIFICATE_STAKE_DEREGISTRATION_CONWAY: {
                    cert_policy = policyForSignTxCertificateStaking(
                        G_context.tx_info.transaction.txSigningMode,
                        certificate_item->certificate.type,
                        &certificate_item->certificate.stakeCredential
                    );
                    switch (cert_policy) {
                        case POLICY_DENY:
                            return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                        case POLICY_SHOW:
                            if (certificate_item->certificate.type == CERTIFICATE_STAKE_REGISTRATION ||
                                certificate_item->certificate.type == CERTIFICATE_STAKE_DEREGISTRATION) {
                                plan->pair_count += 3;  // cert# + type + stake credential
                            } else {
                                plan->pair_count += 4;  // cert# + type + stake credential + deposit
                            }
                            break;
                        case POLICY_HIDE:
                            break;
                    }
                    break;
                }
                case CERTIFICATE_STAKE_DELEGATION: {
                    cert_policy = policyForSignTxCertificateStaking(
                        G_context.tx_info.transaction.txSigningMode,
                        certificate_item->certificate.type,
                        &certificate_item->certificate.stakeCredential
                    );
                    switch (cert_policy) {
                        case POLICY_DENY:
                            return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                        case POLICY_SHOW:
                            plan->pair_count += 4;  // cert# + type + stake credential + pool keyhash
                            break;
                        case POLICY_HIDE:
                            break;
                    }
                    break;
                }
                case CERTIFICATE_VOTE_DELEGATION: {
                    cert_policy = policyForSignTxCertificateVoteDelegation(
                        G_context.tx_info.transaction.txSigningMode,
                        &certificate_item->certificate.stakeCredential,
                        &certificate_item->certificate.drep
                    );
                    switch (cert_policy) {
                        case POLICY_DENY:
                            return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                        case POLICY_SHOW:
                            plan->pair_count += 4;  // cert# + type + stake credential + DRep
                            break;
                        case POLICY_HIDE:
                            break;
                    }
                    break;
                }
                case CERTIFICATE_AUTHORIZE_COMMITTEE_HOT: {
                    cert_policy = policyForSignTxCertificateCommitteeAuth(
                        G_context.tx_info.transaction.txSigningMode,
                        &certificate_item->certificate.coldCredential,
                        &certificate_item->certificate.hotCredential
                    );
                    switch (cert_policy) {
                        case POLICY_DENY:
                            return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                        case POLICY_SHOW:
                            plan->pair_count += 4;  // cert# + type + cold credential + hot credential
                            break;
                        case POLICY_HIDE:
                            break;
                    }
                    break;
                }
                case CERTIFICATE_RESIGN_COMMITTEE_COLD: {
                    cert_policy = policyForSignTxCertificateCommitteeResign(
                        G_context.tx_info.transaction.txSigningMode,
                        &certificate_item->certificate.coldCredential
                    );
                    switch (cert_policy) {
                        case POLICY_DENY:
                            return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                        case POLICY_SHOW:
                            // cert# + type + cold credential + anchor (URL + hash if present)
                            plan->pair_count += 3;
                            if (certificate_item->certificate.anchor.isIncluded) {
                                plan->pair_count += 2;  // anchor URL + anchor hash
                            }
                            break;
                        case POLICY_HIDE:
                            break;
                    }
                    break;
                }
                case CERTIFICATE_DREP_REGISTRATION:
                case CERTIFICATE_DREP_DEREGISTRATION:
                case CERTIFICATE_DREP_UPDATE: {
                    cert_policy = policyForSignTxCertificateDRep(
                        G_context.tx_info.transaction.txSigningMode,
                        &certificate_item->certificate.dRepCredential
                    );
                    switch (cert_policy) {
                        case POLICY_DENY:
                            return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                        case POLICY_SHOW:
                            if (certificate_item->certificate.type == CERTIFICATE_DREP_REGISTRATION) {
                                // cert# + type + DRep credential + deposit + anchor (URL + hash if present)
                                plan->pair_count += 4;
                                if (certificate_item->certificate.anchor.isIncluded) {
                                    plan->pair_count += 2;  // anchor URL + anchor hash
                                }
                            } else if (certificate_item->certificate.type == CERTIFICATE_DREP_DEREGISTRATION) {
                                // cert# + type + DRep credential + deposit
                                plan->pair_count += 4;
                            } else {  // CERTIFICATE_DREP_UPDATE
                                // cert# + type + DRep credential + anchor (URL + hash if present)
                                plan->pair_count += 3;
                                if (certificate_item->certificate.anchor.isIncluded) {
                                    plan->pair_count += 2;  // anchor URL + anchor hash
                                }
                            }
                            break;
                        case POLICY_HIDE:
                            break;
                    }
                    break;
                }
                case CERTIFICATE_STAKE_POOL_REGISTRATION: {
                    pool_owner_counts_t owner_counts = count_pool_owner_nodes(
                        certificate_item->certificate.poolRegistration.poolOwners
                    );
                    if (G_context.tx_info.transaction.txSigningMode ==
                        SIGN_TX_SIGNINGMODE_POOL_REGISTRATION_OWNER) {
                        LEDGER_ASSERT(!G_context.tx_info.pool_owner_path_present,
                                      "Multiple pool registrations in owner mode");
                        if (owner_counts.path_owners == 1) {
                            s_flist_node* owner_node =
                                certificate_item->certificate.poolRegistration.poolOwners;
                            while (owner_node != NULL) {
                                tx_certificate_node_t* owner_item =
                                    (tx_certificate_node_t*) owner_node;
                                const ext_credential_t* owner_cred =
                                    &owner_item->certificate.stakeCredential;
                                if (owner_cred->type == EXT_CREDENTIAL_KEY_PATH) {
                                    G_context.tx_info.pool_owner_path = owner_cred->keyPath;
                                    G_context.tx_info.pool_owner_path_present = true;
                                    break;
                                }
                                owner_node = owner_node->next;
                            }
                            LEDGER_ASSERT(G_context.tx_info.pool_owner_path_present,
                                          "Pool owner path missing");
                        }
                    }
                    cert_policy = policyForSignTxStakePoolRegistrationInit(
                        G_context.tx_info.transaction.txSigningMode,
                        certificate_item->certificate.poolRegistration.numPoolOwners,
                        owner_counts.path_owners
                    );
                    switch (cert_policy) {
                        case POLICY_DENY:
                            return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                        case POLICY_HIDE:
                            break;
                        case POLICY_SHOW: {
                            uint16_t pool_pairs = 2;  // cert# + type

                            security_policy_t pool_id_policy = policyForSignTxStakePoolRegistrationPoolId(
                                G_context.tx_info.transaction.txSigningMode,
                                &certificate_item->certificate.poolId
                            );
                            switch (pool_id_policy) {
                                case POLICY_DENY:
                                    return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                                case POLICY_SHOW:
                                    pool_pairs += 1;
                                    break;
                                case POLICY_HIDE:
                                    break;
                            }

                            security_policy_t vrf_policy = policyForSignTxStakePoolRegistrationVrfKey(
                                G_context.tx_info.transaction.txSigningMode
                            );
                            switch (vrf_policy) {
                                case POLICY_DENY:
                                    return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                                case POLICY_SHOW:
                                    pool_pairs += 1;
                                    break;
                                case POLICY_HIDE:
                                    break;
                            }

                            pool_pairs += 3;  // pledge + cost + profit margin

                            security_policy_t reward_policy = policyForSignTxStakePoolRegistrationRewardAccount(
                                G_context.tx_info.transaction.txSigningMode,
                                G_context.tx_info.transaction.networkId,
                                &certificate_item->certificate.poolRegistration.rewardAccount
                            );
                            switch (reward_policy) {
                                case POLICY_DENY:
                                    return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                                case POLICY_SHOW:
                                    pool_pairs += 1;
                                    break;
                                case POLICY_HIDE:
                                    break;
                            }

                            s_flist_node* owner_node = certificate_item->certificate.poolRegistration.poolOwners;
                            while (owner_node != NULL) {
                                tx_certificate_node_t* owner_item =
                                    (tx_certificate_node_t*) owner_node;
                                ext_credential_t* owner_cred = &owner_item->certificate.stakeCredential;

                                security_policy_t owner_policy = policyForSignTxStakePoolRegistrationOwner(
                                    G_context.tx_info.transaction.txSigningMode,
                                    owner_cred
                                );
                                switch (owner_policy) {
                                    case POLICY_DENY:
                                        return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                                    case POLICY_SHOW:
                                        pool_pairs += 1;
                                        break;
                                    case POLICY_HIDE:
                                        break;
                                }

                                owner_node = owner_node->next;
                            }
                            ASSERT(owner_counts.total_owners ==
                                   certificate_item->certificate.poolRegistration.numPoolOwners);
                            if (owner_counts.total_owners == 0) {
                                pool_pairs += 1;  // "Pool owners" "None"
                            }

                            uint32_t relay_count = 0;
                            s_flist_node* relay_node = certificate_item->certificate.poolRegistration.relays;
                            while (relay_node != NULL) {
                                tx_certificate_node_t* relay_item =
                                    (tx_certificate_node_t*) relay_node;
                                pool_relay_t* relay = (pool_relay_t*) &relay_item->certificate;

                                security_policy_t relay_policy = policyForSignTxStakePoolRegistrationRelay(
                                    G_context.tx_info.transaction.txSigningMode,
                                    relay
                                );
                                switch (relay_policy) {
                                    case POLICY_DENY:
                                        return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                                    case POLICY_HIDE:
                                        break;
                                    case POLICY_SHOW:
                                        pool_pairs += 1;  // relay label
                                        switch (relay->format) {
                                            case RELAY_SINGLE_HOST_IP:
                                                if (!relay->ipv4.isNull) {
                                                    pool_pairs += 1;
                                                }
                                                if (!relay->ipv6.isNull) {
                                                    pool_pairs += 1;
                                                }
                                                if (!relay->port.isNull) {
                                                    pool_pairs += 1;
                                                }
                                                break;
                                            case RELAY_SINGLE_HOST_NAME:
                                                if (relay->dnsNameSize > 0) {
                                                    pool_pairs += 1;
                                                }
                                                if (!relay->port.isNull) {
                                                    pool_pairs += 1;
                                                }
                                                break;
                                            case RELAY_MULTIPLE_HOST_NAME:
                                                if (relay->dnsNameSize > 0) {
                                                    pool_pairs += 1;
                                                }
                                                break;
                                            default:
                                                LEDGER_ASSERT(false, "Unknown relay format type");
                                        }
                                        break;
                                }

                                relay_node = relay_node->next;
                                relay_count++;
                            }
                            ASSERT(relay_count ==
                                   certificate_item->certificate.poolRegistration.numRelays);
                            if (relay_count == 0) {
                                pool_pairs += 1;  // "Pool relays" "None"
                            }

                            if (certificate_item->certificate.poolRegistration.poolMetadataIsNull) {
                                security_policy_t no_metadata_policy = policyForSignTxStakePoolRegistrationNoMetadata();
                                switch (no_metadata_policy) {
                                    case POLICY_DENY:
                                        return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                                    case POLICY_SHOW:
                                        pool_pairs += 1;
                                        break;
                                    case POLICY_HIDE:
                                        break;
                                }
                            } else {
                                security_policy_t metadata_policy = policyForSignTxStakePoolRegistrationMetadata();
                                switch (metadata_policy) {
                                    case POLICY_DENY:
                                        return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                                    case POLICY_SHOW:
                                        pool_pairs += 2;  // metadata url + hash
                                        break;
                                    case POLICY_HIDE:
                                        break;
                                }
                            }

                            plan->pair_count += pool_pairs;
                            break;
                        }
                    }
                    break;
                }
                case CERTIFICATE_STAKE_POOL_RETIREMENT: {
                    cert_policy = policyForSignTxCertificateStakePoolRetirement(
                        G_context.tx_info.transaction.txSigningMode,
                        &certificate_item->certificate.poolCredential,
                        certificate_item->certificate.retirementEpoch
                    );
                    switch (cert_policy) {
                        case POLICY_DENY:
                            return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                        case POLICY_SHOW:
                            plan->pair_count += 4;  // cert# + type + pool ID + retirement epoch
                            break;
                        case POLICY_HIDE:
                            break;
                    }
                    break;
                }
                default:
                    break;
            }

            switch (certificate_item->certificate.type) {
                case CERTIFICATE_STAKE_REGISTRATION:
                case CERTIFICATE_STAKE_DEREGISTRATION: {
                    ext_credential_t stakeCred = _credentialForTxHash(&certificate_item->certificate.stakeCredential);
                    txHashBuilder_addCertificate_stakingOld(
                        &txHashBuilder,
                        certificate_item->certificate.type,
                        &stakeCred
                    );
                    break;
                }
                case CERTIFICATE_STAKE_DELEGATION: {
                    ext_credential_t stakeCred = _credentialForTxHash(&certificate_item->certificate.stakeCredential);
                    txHashBuilder_addCertificate_stakeDelegation(
                        &txHashBuilder,
                        &stakeCred,
                        certificate_item->certificate.poolKeyHash,
                        POOL_KEY_HASH_LENGTH
                    );
                    break;
                }
                case CERTIFICATE_STAKE_REGISTRATION_CONWAY:
                case CERTIFICATE_STAKE_DEREGISTRATION_CONWAY: {
                    ext_credential_t stakeCred = _credentialForTxHash(&certificate_item->certificate.stakeCredential);
                    txHashBuilder_addCertificate_staking(
                        &txHashBuilder,
                        certificate_item->certificate.type,
                        &stakeCred,
                        certificate_item->certificate.deposit
                    );
                    break;
                }
                case CERTIFICATE_STAKE_POOL_RETIREMENT: {
                    const ext_credential_t* poolCred = &certificate_item->certificate.poolCredential;
                    uint8_t poolKeyHash[POOL_KEY_HASH_LENGTH];
                    TRACE("Pool retirement credential type = %d", poolCred->type);
                    switch (poolCred->type) {
                        case EXT_CREDENTIAL_KEY_PATH:
                            TRACE("Pool retirement key path length = %u", poolCred->keyPath.length);
                            bip44_pathToKeyHash(&poolCred->keyPath, poolKeyHash, sizeof(poolKeyHash));
                            break;
                        case EXT_CREDENTIAL_KEY_HASH: {
                            TRACE("Pool retirement credential key hash first byte = %02x", poolCred->keyHash[0]);
                            STATIC_ASSERT(ADDRESS_KEY_HASH_LENGTH == POOL_KEY_HASH_LENGTH,
                                          "pool credential hash size mismatch");
                            memcpy(poolKeyHash, poolCred->keyHash, POOL_KEY_HASH_LENGTH);
                            break;
                        }
                        default:
                            LEDGER_ASSERT(false, "Unsupported pool credential type for retirement");
                    }
                    TRACE("Derived pool key hash first byte = %02x", poolKeyHash[0]);
                    txHashBuilder_addCertificate_poolRetirement(
                        &txHashBuilder,
                        poolKeyHash,
                        POOL_KEY_HASH_LENGTH,
                        certificate_item->certificate.retirementEpoch
                    );
                    break;
                }
                case CERTIFICATE_STAKE_POOL_REGISTRATION: {
                    const certificate_data_t* certData = &certificate_item->certificate;
                    const pool_registration_data_t* poolReg = &certData->poolRegistration;

                    txHashBuilder_poolRegistrationCertificate_enter(
                        &txHashBuilder,
                        poolReg->numPoolOwners,
                        poolReg->numRelays
                    );

                    // Pool Key Hash (from poolId)
                    uint8_t poolKeyHash[POOL_KEY_HASH_LENGTH];
                    if (certData->poolId.keyReferenceType == KEY_REFERENCE_PATH) {
                        bip44_pathToKeyHash(&certData->poolId.path, poolKeyHash, sizeof(poolKeyHash));
                    } else {
                        memcpy(poolKeyHash, certData->poolId.hash, POOL_KEY_HASH_LENGTH);
                    }
                    txHashBuilder_poolRegistrationCertificate_poolKeyHash(
                        &txHashBuilder,
                        poolKeyHash,
                        sizeof(poolKeyHash)
                    );

                    // VRF Key Hash
                    txHashBuilder_poolRegistrationCertificate_vrfKeyHash(
                        &txHashBuilder,
                        certData->vrfKeyHash,
                        sizeof(certData->vrfKeyHash)
                    );

                    // Financials
                    txHashBuilder_poolRegistrationCertificate_financials(
                        &txHashBuilder,
                        poolReg->pledge,
                        poolReg->cost,
                        poolReg->marginNumerator,
                        poolReg->marginDenominator
                    );

                    // Reward Account
                    uint8_t rewardAccountBuf[REWARD_ACCOUNT_LENGTH];
                    rewardAccountToBuffer(
                        &poolReg->rewardAccount,
                        G_context.tx_info.transaction.networkId,
                        rewardAccountBuf
                    );
                    txHashBuilder_poolRegistrationCertificate_rewardAccount(
                        &txHashBuilder,
                        rewardAccountBuf,
                        sizeof(rewardAccountBuf)
                    );

                    // Owners
                    txHashBuilder_addPoolRegistrationCertificate_enterOwners(&txHashBuilder);
                    s_flist_node* owner_node = poolReg->poolOwners;
                    while (owner_node) {
                        tx_certificate_node_t* owner_item = (tx_certificate_node_t*) owner_node;
                        ext_credential_t* cred = &owner_item->certificate.stakeCredential;

                        ext_credential_t ownerCredForHash = _credentialForTxHash(cred);

                        txHashBuilder_addPoolRegistrationCertificate_addOwner(
                            &txHashBuilder,
                            ownerCredForHash.keyHash,
                            sizeof(ownerCredForHash.keyHash)
                        );

                        owner_node = owner_node->next;
                    }

                    // Relays
                    txHashBuilder_addPoolRegistrationCertificate_enterRelays(&txHashBuilder);
                    s_flist_node* relay_node = poolReg->relays;
                    while (relay_node) {
                        tx_certificate_node_t* relay_item_node = (tx_certificate_node_t*) relay_node;
                        pool_relay_t* relay = (pool_relay_t*) &relay_item_node->certificate;

                        txHashBuilder_addPoolRegistrationCertificate_addRelay(
                            &txHashBuilder,
                            relay
                        );

                        relay_node = relay_node->next;
                    }

                    // Metadata
                    if (poolReg->poolMetadataIsNull) {
                        txHashBuilder_addPoolRegistrationCertificate_addPoolMetadata_null(&txHashBuilder);
                    } else {
                        txHashBuilder_addPoolRegistrationCertificate_addPoolMetadata(
                            &txHashBuilder,
                            poolReg->poolMetadata.url,
                            poolReg->poolMetadata.urlSize,
                            poolReg->poolMetadata.hash,
                            POOL_METADATA_HASH_LENGTH
                        );
                    }
                    break;
                }
                case CERTIFICATE_VOTE_DELEGATION: {
                    ext_credential_t stakeCred = _credentialForTxHash(&certificate_item->certificate.stakeCredential);
                    ext_drep_t drep = _drepForTxHash(&certificate_item->certificate.drep);
                    txHashBuilder_addCertificate_voteDelegation(
                        &txHashBuilder,
                        &stakeCred,
                        &drep
                    );
                    break;
                }
                case CERTIFICATE_AUTHORIZE_COMMITTEE_HOT: {
                    ext_credential_t coldCred = _credentialForTxHash(&certificate_item->certificate.coldCredential);
                    ext_credential_t hotCred = _credentialForTxHash(&certificate_item->certificate.hotCredential);
                    txHashBuilder_addCertificate_committeeAuthHot(
                        &txHashBuilder,
                        &coldCred,
                        &hotCred
                    );
                    break;
                }
                case CERTIFICATE_RESIGN_COMMITTEE_COLD: {
                    ext_credential_t coldCred = _credentialForTxHash(&certificate_item->certificate.coldCredential);
                    txHashBuilder_addCertificate_committeeResign(
                        &txHashBuilder,
                        &coldCred,
                        &certificate_item->certificate.anchor
                    );
                    break;
                }
                case CERTIFICATE_DREP_REGISTRATION: {
                    ext_credential_t drepCred = _credentialForTxHash(&certificate_item->certificate.dRepCredential);
                    txHashBuilder_addCertificate_dRepRegistration(
                        &txHashBuilder,
                        &drepCred,
                        certificate_item->certificate.deposit,
                        &certificate_item->certificate.anchor
                    );
                    break;
                }
                case CERTIFICATE_DREP_DEREGISTRATION: {
                    ext_credential_t drepCred = _credentialForTxHash(&certificate_item->certificate.dRepCredential);
                    txHashBuilder_addCertificate_dRepDeregistration(
                        &txHashBuilder,
                        &drepCred,
                        certificate_item->certificate.deposit
                    );
                    break;
                }
                case CERTIFICATE_DREP_UPDATE: {
                    ext_credential_t drepCred = _credentialForTxHash(&certificate_item->certificate.dRepCredential);
                    txHashBuilder_addCertificate_dRepUpdate(
                        &txHashBuilder,
                        &drepCred,
                        &certificate_item->certificate.anchor
                    );
                    break;
                }
                default:
                    // Unsupported certificate type in validation phase
                    LEDGER_ASSERT(false, "Unsupported certificate type in tx_prepare");
            }

            certificate_node = certificate_node->next;
        }
    }

    if (G_context.tx_info.transaction.num_withdrawals > 0) {
        txHashBuilder_enterWithdrawals(&txHashBuilder);

        // Track previous reward account for CBOR canonical ordering validation
        uint8_t previousRewardAccount[REWARD_ACCOUNT_LENGTH];
        explicit_bzero(previousRewardAccount, REWARD_ACCOUNT_LENGTH);
        bool isFirstWithdrawal = true;

        s_flist_node *withdrawal_node = G_context.tx_info.transaction.withdrawals;
        while (withdrawal_node != NULL) {
            tx_withdrawal_node_t *withdrawal_item =
                (tx_withdrawal_node_t *) withdrawal_node;

            security_policy_t withdrawal_policy = policyForSignTxWithdrawal(
                G_context.tx_info.transaction.txSigningMode,
                &withdrawal_item->withdrawal.stakeCredential,
                &G_context.tx_info.warning_bits
            );

            switch (withdrawal_policy) {
                case POLICY_DENY:
                    TRACE("Withdrawal security policy denied");
                    return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                case POLICY_SHOW:
                    plan->pair_count += 3;
                    break;
                case POLICY_HIDE:
                    break;
            }

            uint8_t reward_address[REWARD_ACCOUNT_LENGTH];
            size_t reward_addr_len = 0;
            switch (withdrawal_item->withdrawal.stakeCredential.type) {
                case EXT_CREDENTIAL_KEY_PATH:
                    reward_addr_len = constructRewardAddressFromKeyPath(
                        &withdrawal_item->withdrawal.stakeCredential.keyPath,
                        G_context.tx_info.transaction.networkId,
                        reward_address,
                        sizeof(reward_address)
                    );
                    break;
                case EXT_CREDENTIAL_KEY_HASH:
                    reward_addr_len = constructRewardAddressFromHash(
                        G_context.tx_info.transaction.networkId,
                        REWARD_HASH_SOURCE_KEY,
                        withdrawal_item->withdrawal.stakeCredential.keyHash,
                        ADDRESS_KEY_HASH_LENGTH,
                        reward_address,
                        sizeof(reward_address)
                    );
                    break;
                case EXT_CREDENTIAL_SCRIPT_HASH:
                    reward_addr_len = constructRewardAddressFromHash(
                        G_context.tx_info.transaction.networkId,
                        REWARD_HASH_SOURCE_SCRIPT,
                        withdrawal_item->withdrawal.stakeCredential.scriptHash,
                        SCRIPT_HASH_LENGTH,
                        reward_address,
                        sizeof(reward_address)
                    );
                    break;
                default:
                    LEDGER_ASSERT(false, "Unknown withdrawal credential type");
            }

            LEDGER_ASSERT(reward_addr_len == REWARD_ACCOUNT_LENGTH, "Invalid reward address length");

            // Validate CBOR canonical ordering of withdrawal map keys
            if (!isFirstWithdrawal) {
                if (!cbor_mapKeyFulfillsCanonicalOrdering(
                        previousRewardAccount,
                        REWARD_ACCOUNT_LENGTH,
                        reward_address,
                        reward_addr_len)) {
                    TRACE("Withdrawals not in canonical order");
                    return send_swo_and_reset(SWO_TX_PARSING_FAIL_WITHDRAWALS);
                }
            }

            // Update for next iteration
            memmove(previousRewardAccount, reward_address, reward_addr_len);
            isFirstWithdrawal = false;

            txHashBuilder_addWithdrawal(&txHashBuilder,
                                       reward_address,
                                       reward_addr_len,
                                       withdrawal_item->withdrawal.amount);

            withdrawal_node = withdrawal_node->next;
        }
    }

    if (G_context.tx_info.transaction.includeAuxDataHash) {
        txHashBuilder_addAuxData(&txHashBuilder,
                                 G_context.tx_info.transaction.auxDataHash,
                                 AUX_DATA_HASH_LENGTH);
    }

    if (G_context.tx_info.transaction.includeValidityIntervalStart) {
        txHashBuilder_addValidityIntervalStart(&txHashBuilder,
                                               G_context.tx_info.transaction.validityIntervalStart);
    }

    if (G_context.tx_info.transaction.num_mint_asset_groups > 0) {
        txHashBuilder_enterMint(&txHashBuilder);
        txHashBuilder_addMint_topLevelData(&txHashBuilder,
                                           G_context.tx_info.transaction.num_mint_asset_groups);

        s_flist_node *mint_node = G_context.tx_info.transaction.mint_asset_groups;
        while (mint_node != NULL) {
            mint_asset_group_node_t *mint_item =
                (mint_asset_group_node_t *) mint_node;

            txHashBuilder_addMint_tokenGroup(&txHashBuilder,
                                             mint_item->asset_group.policyId,
                                             MINTING_POLICY_ID_LENGTH,
                                             mint_item->asset_group.numTokens);

            // Iterate through linked list of tokens instead of array
            s_flist_node *token_node = mint_item->asset_group.tokens;
            while (token_node != NULL) {
                mint_token_node_t *token_item = (mint_token_node_t *) token_node;
                mint_token_t *token = &token_item->token;
                txHashBuilder_addMint_token(&txHashBuilder,
                                            token->assetName,
                                            token->assetNameLen,
                                            (uint64_t)token->amount);
                token_node = token_node->next;
            }

            mint_node = mint_node->next;
        }
    }

    // key 11: script data hash
    if (G_context.tx_info.transaction.includeScriptDataHash) {
        security_policy_t policy = policyForSignTxScriptDataHash(
            G_context.tx_info.transaction.txSigningMode
        );

        switch (policy) {
            case POLICY_DENY:
                TRACE("Script data hash security policy denied");
                return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
            case POLICY_SHOW:
                plan->pair_count += 1;  // Display script data hash
                break;
            case POLICY_HIDE:
                break;
        }

        txHashBuilder_addScriptDataHash(&txHashBuilder,
                                        G_context.tx_info.transaction.scriptDataHash,
                                        SCRIPT_DATA_HASH_LENGTH);
    }

    // key 13: collateral inputs
    if (G_context.tx_info.transaction.num_collateral_inputs > 0) {
        txHashBuilder_enterCollateralInputs(&txHashBuilder);

        s_flist_node *collateral_input_node = G_context.tx_info.transaction.collateral_inputs;
        while (collateral_input_node != NULL) {
            tx_collateral_input_node_t *input_item =
                (tx_collateral_input_node_t *) collateral_input_node;

            security_policy_t collateral_input_policy = policyForSignTxCollateralInput(
                G_context.tx_info.transaction.txSigningMode,
                G_context.tx_info.transaction.includeTotalCollateral,
                &input_item->input
            );

            switch (collateral_input_policy) {
                case POLICY_DENY:
                    TRACE("Collateral input security policy denied");
                    return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                case POLICY_SHOW:
                    plan->pair_count += 1;  // Display collateral input
                    break;
                case POLICY_HIDE:
                    break;
            }

            txHashBuilder_addCollateralInput(&txHashBuilder, &input_item->input);
            collateral_input_node = collateral_input_node->next;
        }
    }

    // key 14: required signers
    if (G_context.tx_info.transaction.num_required_signers > 0) {
        txHashBuilder_enterRequiredSigners(&txHashBuilder);

        s_flist_node *required_signer_node = G_context.tx_info.transaction.required_signers;
        while (required_signer_node != NULL) {
            tx_required_signer_node_t *signer_item =
                (tx_required_signer_node_t *) required_signer_node;

            security_policy_t signer_policy = policyForSignTxRequiredSigner(
                G_context.tx_info.transaction.txSigningMode,
                &signer_item->required_signer
            );

            switch (signer_policy) {
                case POLICY_DENY:
                    TRACE("Required signer security policy denied");
                    return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                case POLICY_SHOW:
                    plan->pair_count += 1;  // Display required signer
                    break;
                case POLICY_HIDE:
                    break;
            }

            // Add to hash - derive key hash if path provided
            uint8_t keyHash[ADDRESS_KEY_HASH_LENGTH];
            if (signer_item->required_signer.type == REQUIRED_SIGNER_WITH_PATH) {
                bip44_pathToKeyHash(&signer_item->required_signer.keyPath,
                                   keyHash, sizeof(keyHash));
            } else {
                ASSERT(signer_item->required_signer.type == REQUIRED_SIGNER_WITH_HASH);
                memmove(keyHash, signer_item->required_signer.keyHash, sizeof(keyHash));
            }
            txHashBuilder_addRequiredSigner(&txHashBuilder, keyHash, sizeof(keyHash));

            required_signer_node = required_signer_node->next;
        }
    }

    // key 15: network ID
    if (G_context.tx_info.transaction.includeNetworkId) {
        txHashBuilder_addNetworkId(&txHashBuilder, G_context.tx_info.transaction.networkId);
        // No UI display - network already shown during init
    }

    // key 16: collateral return output
    if (G_context.tx_info.transaction.includeCollateralOutput) {
        tx_output_description_t collateral_desc = {0};
        collateral_desc.format = G_context.tx_info.transaction.collateral_output.format;
        collateral_desc.amount = G_context.tx_info.transaction.collateral_output.adaAmount;
        collateral_desc.numAssetGroups = G_context.tx_info.transaction.collateral_output.numAssetGroups;
        collateral_desc.includeDatum = G_context.tx_info.transaction.collateral_output.datum.hasDatum;
        collateral_desc.includeRefScript = G_context.tx_info.transaction.collateral_output.refScript.hasRefScript;

        if (G_context.tx_info.transaction.collateral_output.destination.type == DESTINATION_THIRD_PARTY) {
            collateral_desc.destination.type = DESTINATION_THIRD_PARTY;
            collateral_desc.destination.address.buffer =
                G_context.tx_info.transaction.collateral_output.destination.address.buffer;
            collateral_desc.destination.address.size =
                G_context.tx_info.transaction.collateral_output.destination.address.size;
        } else {
            collateral_desc.destination.type = DESTINATION_DEVICE_OWNED;
            collateral_desc.destination.params =
                &G_context.tx_info.transaction.collateral_output.destination.params;
        }

        security_policy_t collateral_policy;
        if (collateral_desc.destination.type == DESTINATION_THIRD_PARTY) {
            collateral_policy = policyForSignTxCollateralOutputAddressBytes(
                &collateral_desc,
                G_context.tx_info.transaction.txSigningMode,
                G_context.tx_info.transaction.networkId,
                G_context.tx_info.transaction.protocolMagic
            );
        } else {
            collateral_policy = policyForSignTxCollateralOutputAddressParams(
                &collateral_desc,
                G_context.tx_info.transaction.txSigningMode,
                G_context.tx_info.transaction.networkId,
                G_context.tx_info.transaction.protocolMagic,
                G_context.tx_info.transaction.includeTotalCollateral
            );
        }

        security_policy_t collateral_ada_policy =
            policyForSignTxCollateralOutputAdaAmount(
                collateral_policy,
                G_context.tx_info.transaction.includeTotalCollateral
            );
        security_policy_t collateral_tokens_policy =
            policyForSignTxCollateralOutputTokens(
                collateral_policy,
                &collateral_desc
            );

        switch (collateral_policy) {
            case POLICY_DENY:
                TRACE("Collateral output security policy denied");
                return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
            case POLICY_SHOW: {
                plan->pair_count += 1;  // collateral address
                // For device-owned collateral addresses, add 2 more pairs (payment info + staking info)
                if (G_context.tx_info.transaction.collateral_output.destination.type == DESTINATION_DEVICE_OWNED) {
                    plan->pair_count += 2;
                }
                if (collateral_ada_policy == POLICY_SHOW) {
                    plan->pair_count += 1;  // collateral amount
                }
                if (collateral_tokens_policy == POLICY_SHOW &&
                    G_context.tx_info.transaction.collateral_output.assetGroups != NULL) {
                    uint16_t collateral_group_count = 0;
                    s_flist_node *collateral_group_node =
                        G_context.tx_info.transaction.collateral_output.assetGroups;
                    while (collateral_group_node != NULL) {
                        output_asset_group_node_t *group_node =
                            (output_asset_group_node_t *) collateral_group_node;
                        output_asset_group_t *group = &group_node->asset_group;
                        s_flist_node *token_node = group->tokens;
                        while (token_node != NULL) {
                            plan->pair_count += 2;
                            token_node = token_node->next;
                        }
                        collateral_group_count++;
                        collateral_group_node = collateral_group_node->next;
                    }
                    LEDGER_ASSERT(collateral_group_count ==
                                  G_context.tx_info.transaction.collateral_output.numAssetGroups,
                                  "Collateral asset group count mismatch");
                }
                break;
            }
            case POLICY_HIDE:
                break;
        }

        if (collateral_desc.destination.type == DESTINATION_THIRD_PARTY) {
            txHashBuilder_addCollateralOutput(&txHashBuilder, &collateral_desc);
        } else {
            uint8_t *address_bytes = (uint8_t *) app_mem_alloc(MAX_ADDRESS_LENGTH);
            if (address_bytes == NULL) {
                return send_swo_and_reset(SWO_INSUFFICIENT_MEMORY);
            }

            size_t address_size = deriveAddress(
                &G_context.tx_info.transaction.collateral_output.destination.params,
                address_bytes,
                MAX_ADDRESS_LENGTH
            );

            if (address_size == 0 || address_size > MAX_ADDRESS_LENGTH) {
                app_mem_free(address_bytes);
                return send_swo_and_reset(SWO_INCORRECT_DATA);
            }

            collateral_desc.destination.type = DESTINATION_THIRD_PARTY;
            collateral_desc.destination.address.buffer = address_bytes;
            collateral_desc.destination.address.size = address_size;
            txHashBuilder_addCollateralOutput(&txHashBuilder, &collateral_desc);
            app_mem_free(address_bytes);
        }

        uint16_t collateral_group_count = 0;
        s_flist_node *collateral_group_node = G_context.tx_info.transaction.collateral_output.assetGroups;
        while (collateral_group_node != NULL) {
            output_asset_group_node_t *group_node =
                (output_asset_group_node_t *) collateral_group_node;
            output_asset_group_t *group = &group_node->asset_group;
            txHashBuilder_addCollateralOutput_tokenGroup(&txHashBuilder,
                                                        group->policyId,
                                                        MINTING_POLICY_ID_LENGTH,
                                                        group->numTokens);

            s_flist_node *token_node = group->tokens;
            while (token_node != NULL) {
                output_token_node_t *token_item = (output_token_node_t *) token_node;
                output_token_t *token = &token_item->token_data;
                txHashBuilder_addCollateralOutput_token(&txHashBuilder,
                                                       token->assetName,
                                                       token->assetNameLen,
                                                       (uint64_t)token->amount);
                token_node = token_node->next;
            }
            collateral_group_count++;
            collateral_group_node = collateral_group_node->next;
        }
        LEDGER_ASSERT(collateral_group_count ==
                      G_context.tx_info.transaction.collateral_output.numAssetGroups,
                      "Collateral asset group count mismatch");
    }

    // key 17: total collateral
    if (G_context.tx_info.transaction.includeTotalCollateral) {
        security_policy_t policy = policyForSignTxTotalCollateral();

        switch (policy) {
            case POLICY_DENY:
                TRACE("Total collateral security policy denied");
                return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
            case POLICY_SHOW:
                plan->pair_count += 1;  // Display total collateral amount
                break;
            case POLICY_HIDE:
                break;
        }

        txHashBuilder_addTotalCollateral(&txHashBuilder, G_context.tx_info.transaction.totalCollateral);
    }

    // key 18: reference inputs
    if (G_context.tx_info.transaction.num_reference_inputs > 0) {
        txHashBuilder_enterReferenceInputs(&txHashBuilder);

        s_flist_node *reference_input_node = G_context.tx_info.transaction.reference_inputs;
        while (reference_input_node != NULL) {
            tx_input_node_t *input_item = (tx_input_node_t *) reference_input_node;

            security_policy_t reference_input_policy = policyForSignTxReferenceInput(
                G_context.tx_info.transaction.txSigningMode,
                &input_item->input);

            switch (reference_input_policy) {
                case POLICY_DENY:
                    TRACE("Reference input security policy denied");
                    return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                case POLICY_SHOW:
                    plan->pair_count += 1;  // Display reference input
                    break;
                case POLICY_HIDE:
                    break;
            }

            txHashBuilder_addReferenceInput(&txHashBuilder, &input_item->input);
            reference_input_node = reference_input_node->next;
        }
    }

    // key 19: voting procedures
    if (G_context.tx_info.transaction.num_voters > 0) {
        txHashBuilder_enterVotingProcedures(&txHashBuilder);

        uint8_t previous_voter_key[MAX_CBOR_VOTER_MAP_KEY_SIZE];
        size_t previous_voter_key_len = 0;
        bool has_previous_voter_key = false;

        s_flist_node *voter_node = G_context.tx_info.transaction.voting_procedures;
        while (voter_node != NULL) {
            voter_votes_list_item_t *voter_item = (voter_votes_list_item_t *) voter_node;

            // Security policy check for this voter
            security_policy_t voter_policy = policyForSignTxVotingProcedure(
                G_context.tx_info.transaction.txSigningMode,
                &voter_item->voter_votes_data.voter
            );

            switch (voter_policy) {
                case POLICY_DENY:
                    return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                case POLICY_SHOW:
                    plan->pair_count++;  // Display voter once
                    // Count UI pairs for all votes
                    s_flist_node *vote_node = voter_item->voter_votes_data.votes;
                    while (vote_node != NULL) {
                        vote_list_item_t *vote_item = (vote_list_item_t *) vote_node;
                        plan->pair_count += 3;  // gov action tx hash, gov action index, vote option
                        if (vote_item->vote_data.anchor.isIncluded) {
                            plan->pair_count += 2;  // anchor URL + anchor hash
                        }
                        vote_node = vote_node->next;
                    }
                    break;
                case POLICY_HIDE:
                    break;
            }

            // Convert voter for hash building (KEY_PATH -> KEY_HASH)
            ext_voter_t voter_for_hash = _voterForTxHash(&voter_item->voter_votes_data.voter);

            uint8_t voter_key[MAX_CBOR_VOTER_MAP_KEY_SIZE];
            size_t voter_key_len = 0;
            if (!txHashBuilder_serializeVoterKey(
                    &voter_for_hash,
                    voter_key,
                    sizeof(voter_key),
                    &voter_key_len)) {
                TRACE("Failed to serialize voter key");
                return send_swo_and_reset(SWO_TX_PARSING_FAIL_VOTING_PROCEDURES);
            }

            if (has_previous_voter_key &&
                !cbor_mapKeyFulfillsCanonicalOrdering(
                    previous_voter_key,
                    previous_voter_key_len,
                    voter_key,
                    voter_key_len)) {
                TRACE("Voting procedures not in canonical order");
                return send_swo_and_reset(SWO_TX_PARSING_FAIL_VOTING_PROCEDURES);
            }

            memcpy(previous_voter_key, voter_key, voter_key_len);
            previous_voter_key_len = voter_key_len;
            has_previous_voter_key = true;

            // Add voter with all their votes to the hash
            txHashBuilder_addVoter(&txHashBuilder,
                                   &voter_for_hash,
                                   voter_item->voter_votes_data.numVotes);

            s_flist_node *vote_node = voter_item->voter_votes_data.votes;
            uint8_t previous_vote_key[MAX_CBOR_GOV_ACTION_MAP_KEY_SIZE];
            size_t previous_vote_key_len = 0;
            bool has_previous_vote_key = false;
            while (vote_node != NULL) {
                vote_list_item_t *vote_item = (vote_list_item_t *) vote_node;

                uint8_t gov_action_key[MAX_CBOR_GOV_ACTION_MAP_KEY_SIZE];
                size_t gov_action_key_len = 0;

                if (!txHashBuilder_serializeGovActionKey(
                        &vote_item->vote_data.govActionId,
                        gov_action_key,
                        sizeof(gov_action_key),
                        &gov_action_key_len)) {
                    TRACE("Failed to serialize gov action key");
                    return send_swo_and_reset(SWO_TX_PARSING_FAIL_VOTING_PROCEDURES);
                }

                if (has_previous_vote_key &&
                    !cbor_mapKeyFulfillsCanonicalOrdering(
                        previous_vote_key,
                        previous_vote_key_len,
                        gov_action_key,
                        gov_action_key_len)) {
                    TRACE("Votes not in canonical order");
                    return send_swo_and_reset(SWO_TX_PARSING_FAIL_VOTING_PROCEDURES);
                }

                memcpy(previous_vote_key, gov_action_key, gov_action_key_len);
                previous_vote_key_len = gov_action_key_len;
                has_previous_vote_key = true;

                voting_procedure_t voting_procedure = {
                    .vote = vote_item->vote_data.voteOption,
                    .anchor = vote_item->vote_data.anchor
                };

                txHashBuilder_addVote(&txHashBuilder,
                                     &vote_item->vote_data.govActionId,
                                     &voting_procedure);

                vote_node = vote_node->next;
            }

            voter_node = voter_node->next;
        }
    }

    // key 21: treasury
    if (G_context.tx_info.transaction.includeTreasury) {
        security_policy_t treasury_policy = policyForSignTxTreasury(
            G_context.tx_info.transaction.txSigningMode,
            G_context.tx_info.transaction.treasury
        );
        switch (treasury_policy) {
            case POLICY_DENY:
                return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
            case POLICY_SHOW:
                plan->pair_count++;
                break;
            case POLICY_HIDE:
                break;
        }
        txHashBuilder_addTreasury(&txHashBuilder, G_context.tx_info.transaction.treasury);
    }

    // key 22: donation
    if (G_context.tx_info.transaction.includeDonation) {
        security_policy_t donation_policy = policyForSignTxDonation(
            G_context.tx_info.transaction.txSigningMode,
            G_context.tx_info.transaction.donation
        );
        switch (donation_policy) {
            case POLICY_DENY:
                return send_swo_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
            case POLICY_SHOW:
                plan->pair_count++;
                break;
            case POLICY_HIDE:
                break;
        }
        txHashBuilder_addDonation(&txHashBuilder, G_context.tx_info.transaction.donation);
    }

    txHashBuilder_finalize(&txHashBuilder,
                          G_context.tx_info.tx_hash,
                          sizeof(G_context.tx_info.tx_hash));

    TRACE("Hash: %.*H", sizeof(G_context.tx_info.tx_hash), G_context.tx_info.tx_hash);

    // TODO: implement streaming review flow and remove this assertion once we're handling overflow.
    LEDGER_ASSERT(plan->pair_count <= UI_PAIR_LIMIT, "Need streaming UI fallback");
    LEDGER_ASSERT(plan->pair_count <= UINT8_MAX, "Pair count overflow");

    return SWO_SUCCESS;
}
