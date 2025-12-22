#include <stddef.h>  // NULL
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "tx_prepare.h"

#include "os.h"
#include "app_tokens/app_tokens.h"
#include "cardano_swo.h"
#include "cardano_settings.h"
#include "globals.h"
#include "addressUtils/bip44.h"
#include "addressUtils/addressUtilsShelley.h"
#include "tx_output_types.h"
#include "transaction/tx_hash_builder.h"
#include "memory/mem.h"
#include "securityPolicy/securityPolicy.h"
#include "securityPolicy/securityWarnings.h"
#include "utils/assert.h"
#include "io.h"
#include "utils/cardano_os_utils.h"

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

int compute_tx_hash_and_plan_ui(tx_ui_plan_t* plan) {
    LEDGER_ASSERT(G_context.state.tx_state == TX_STATE_PARSED, "Hash planning invoked at wrong state");
    LEDGER_ASSERT(plan != NULL, "NULL plan");

    TRACE("Expert mode: %d", is_expert_mode());

    plan->pair_count = 2;  // fee + tx hash
    if (G_context.tx_info.transaction.includeTtl) {
        security_policy_t ttl_policy = policyForSignTxTtl(G_context.tx_info.transaction.ttl);
        switch (ttl_policy) {
            case POLICY_DENY:
                return send_error_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
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
                return send_error_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
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
                return send_error_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
            case POLICY_SHOW: {
                // summary entry
                plan->pair_count++;
                // each minted token contributes fingerprint + amount
                s_flist_node* mint_node = G_context.tx_info.transaction.mint_asset_groups;
                while (mint_node != NULL) {
                    mint_asset_group_list_item_t* mint_item =
                        (mint_asset_group_list_item_t*) mint_node;
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
                      false,
                      G_context.tx_info.transaction.includeValidityIntervalStart,
                      G_context.tx_info.transaction.num_mint_asset_groups > 0,
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

    txHashBuilder_enterInputs(&txHashBuilder);
    s_flist_node *input_node = G_context.tx_info.transaction.inputs;
    while (input_node != NULL) {
        tx_input_list_item_t *item = (tx_input_list_item_t *) input_node;
        txHashBuilder_addInput(&txHashBuilder, (const tx_input_t*)&item->input_data);
        input_node = input_node->next;
    }

    txHashBuilder_enterOutputs(&txHashBuilder);
    s_flist_node *output_node = G_context.tx_info.transaction.outputs;
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
        } else {
            output_desc.destination.type = DESTINATION_DEVICE_OWNED;
            output_desc.destination.params = &output_item->output_data.destination.params;
        }

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
                return send_error_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
            case POLICY_SHOW:
                plan->pair_count += 3;
                break;
            case POLICY_HIDE:
                break;
        }

        if (output_item->output_data.destination.type == DESTINATION_THIRD_PARTY) {
            txHashBuilder_addOutput_topLevelData(&txHashBuilder, &output_desc);
        } else {
            uint8_t *address_bytes = (uint8_t *) app_mem_alloc(MAX_ADDRESS_LENGTH);
            if (address_bytes == NULL) {
                return send_error_and_reset(SWO_TX_PARSING_FAIL);
            }

            size_t address_size = deriveAddress(
                &output_item->output_data.destination.params,
                address_bytes,
                MAX_ADDRESS_LENGTH
            );

            if (address_size == 0 || address_size > MAX_ADDRESS_LENGTH) {
                app_mem_free(address_bytes);
                return send_error_and_reset(SWO_TX_PARSING_FAIL);
            }

            output_desc.destination.type = DESTINATION_THIRD_PARTY;
            output_desc.destination.address.buffer = address_bytes;
            output_desc.destination.address.size = address_size;
            txHashBuilder_addOutput_topLevelData(&txHashBuilder, &output_desc);
            app_mem_free(address_bytes);
        }

        for (uint16_t ag = 0; ag < output_item->output_data.numAssetGroups; ag++) {
            asset_group_t *group = &output_item->output_data.assetGroups[ag];
            txHashBuilder_addOutput_tokenGroup(&txHashBuilder,
                                               group->policyId,
                                               MINTING_POLICY_ID_LENGTH,
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

    txHashBuilder_addFee(&txHashBuilder, G_context.tx_info.transaction.fee);
    security_policy_t fee_policy = policyForSignTxFee(G_context.tx_info.transaction.txSigningMode,
                                                      G_context.tx_info.transaction.fee,
                                                      &G_context.tx_info.warning_bits);
    switch (fee_policy) {
        case POLICY_DENY:
            return send_error_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
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
            tx_certificate_list_item_t *certificate_item =
                (tx_certificate_list_item_t *) certificate_node;

            // First check generic policy (only for DENY - validates certificate type is allowed in this signing mode)
            security_policy_t generic_policy = policyForSignTxCertificate(
                G_context.tx_info.transaction.txSigningMode,
                certificate_item->certificate_data.type
            );
            switch (generic_policy) {
                case POLICY_DENY:
                    return send_error_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                case POLICY_SHOW:
                case POLICY_HIDE:
                    break;  // Continue to type-specific policy check
            }

            // Then check type-specific policy for SHOW/HIDE decision and UI pair counting
            security_policy_t cert_policy = POLICY_HIDE;

            switch (certificate_item->certificate_data.type) {
                case CERTIFICATE_STAKE_REGISTRATION:
                case CERTIFICATE_STAKE_DEREGISTRATION:
                case CERTIFICATE_STAKE_REGISTRATION_CONWAY:
                case CERTIFICATE_STAKE_DEREGISTRATION_CONWAY: {
                    cert_policy = policyForSignTxCertificateStaking(
                        G_context.tx_info.transaction.txSigningMode,
                        certificate_item->certificate_data.type,
                        &certificate_item->certificate_data.stakeCredential
                    );
                    switch (cert_policy) {
                        case POLICY_DENY:
                            return send_error_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                        case POLICY_SHOW:
                            if (certificate_item->certificate_data.type == CERTIFICATE_STAKE_REGISTRATION ||
                                certificate_item->certificate_data.type == CERTIFICATE_STAKE_DEREGISTRATION) {
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
                        certificate_item->certificate_data.type,
                        &certificate_item->certificate_data.stakeCredential
                    );
                    switch (cert_policy) {
                        case POLICY_DENY:
                            return send_error_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
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
                        &certificate_item->certificate_data.stakeCredential,
                        &certificate_item->certificate_data.drep
                    );
                    switch (cert_policy) {
                        case POLICY_DENY:
                            return send_error_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                        case POLICY_SHOW:
                            plan->pair_count += 3;  // cert# + type + stake credential (DRep not shown)
                            break;
                        case POLICY_HIDE:
                            break;
                    }
                    break;
                }
                case CERTIFICATE_AUTHORIZE_COMMITTEE_HOT: {
                    cert_policy = policyForSignTxCertificateCommitteeAuth(
                        G_context.tx_info.transaction.txSigningMode,
                        &certificate_item->certificate_data.coldCredential,
                        &certificate_item->certificate_data.hotCredential
                    );
                    switch (cert_policy) {
                        case POLICY_DENY:
                            return send_error_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                        case POLICY_SHOW:
                            plan->pair_count += 2;  // cert# + type only
                            break;
                        case POLICY_HIDE:
                            break;
                    }
                    break;
                }
                case CERTIFICATE_RESIGN_COMMITTEE_COLD: {
                    cert_policy = policyForSignTxCertificateCommitteeResign(
                        G_context.tx_info.transaction.txSigningMode,
                        &certificate_item->certificate_data.coldCredential
                    );
                    switch (cert_policy) {
                        case POLICY_DENY:
                            return send_error_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                        case POLICY_SHOW:
                            plan->pair_count += 2;  // cert# + type only
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
                        &certificate_item->certificate_data.dRepCredential
                    );
                    switch (cert_policy) {
                        case POLICY_DENY:
                            return send_error_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                        case POLICY_SHOW:
                            plan->pair_count += 2;  // cert# + type only
                            break;
                        case POLICY_HIDE:
                            break;
                    }
                    break;
                }
                case CERTIFICATE_STAKE_POOL_RETIREMENT: {
                    cert_policy = policyForSignTxCertificateStakePoolRetirement(
                        G_context.tx_info.transaction.txSigningMode,
                        &certificate_item->certificate_data.poolCredential,
                        certificate_item->certificate_data.retirementEpoch
                    );
                    switch (cert_policy) {
                        case POLICY_DENY:
                            return send_error_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                        case POLICY_SHOW:
                            plan->pair_count += 3;  // cert# + type + retirement epoch
                            break;
                        case POLICY_HIDE:
                            break;
                    }
                    break;
                }
                default:
                    break;
            }

            switch (certificate_item->certificate_data.type) {
                case CERTIFICATE_STAKE_REGISTRATION:
                case CERTIFICATE_STAKE_DEREGISTRATION: {
                    ext_credential_t stakeCred = _credentialForTxHash(&certificate_item->certificate_data.stakeCredential);
                    txHashBuilder_addCertificate_stakingOld(
                        &txHashBuilder,
                        certificate_item->certificate_data.type,
                        &stakeCred
                    );
                    break;
                }
                case CERTIFICATE_STAKE_DELEGATION: {
                    ext_credential_t stakeCred = _credentialForTxHash(&certificate_item->certificate_data.stakeCredential);
                    txHashBuilder_addCertificate_stakeDelegation(
                        &txHashBuilder,
                        &stakeCred,
                        certificate_item->certificate_data.poolKeyHash,
                        POOL_KEY_HASH_LENGTH
                    );
                    break;
                }
                case CERTIFICATE_STAKE_REGISTRATION_CONWAY:
                case CERTIFICATE_STAKE_DEREGISTRATION_CONWAY: {
                    ext_credential_t stakeCred = _credentialForTxHash(&certificate_item->certificate_data.stakeCredential);
                    txHashBuilder_addCertificate_staking(
                        &txHashBuilder,
                        certificate_item->certificate_data.type,
                        &stakeCred,
                        certificate_item->certificate_data.deposit
                    );
                    break;
                }
                case CERTIFICATE_STAKE_POOL_RETIREMENT: {
                    const ext_credential_t* poolCred = &certificate_item->certificate_data.poolCredential;
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
                        certificate_item->certificate_data.retirementEpoch
                    );
                    break;
                }
                case CERTIFICATE_VOTE_DELEGATION: {
                    ext_credential_t stakeCred = _credentialForTxHash(&certificate_item->certificate_data.stakeCredential);
                    ext_drep_t drep = _drepForTxHash(&certificate_item->certificate_data.drep);
                    txHashBuilder_addCertificate_voteDelegation(
                        &txHashBuilder,
                        &stakeCred,
                        &drep
                    );
                    break;
                }
                case CERTIFICATE_AUTHORIZE_COMMITTEE_HOT: {
                    ext_credential_t coldCred = _credentialForTxHash(&certificate_item->certificate_data.coldCredential);
                    ext_credential_t hotCred = _credentialForTxHash(&certificate_item->certificate_data.hotCredential);
                    txHashBuilder_addCertificate_committeeAuthHot(
                        &txHashBuilder,
                        &coldCred,
                        &hotCred
                    );
                    break;
                }
                case CERTIFICATE_RESIGN_COMMITTEE_COLD: {
                    ext_credential_t coldCred = _credentialForTxHash(&certificate_item->certificate_data.coldCredential);
                    txHashBuilder_addCertificate_committeeResign(
                        &txHashBuilder,
                        &coldCred,
                        &certificate_item->certificate_data.anchor
                    );
                    break;
                }
                case CERTIFICATE_DREP_REGISTRATION: {
                    ext_credential_t drepCred = _credentialForTxHash(&certificate_item->certificate_data.dRepCredential);
                    txHashBuilder_addCertificate_dRepRegistration(
                        &txHashBuilder,
                        &drepCred,
                        certificate_item->certificate_data.deposit,
                        &certificate_item->certificate_data.anchor
                    );
                    break;
                }
                case CERTIFICATE_DREP_DEREGISTRATION: {
                    ext_credential_t drepCred = _credentialForTxHash(&certificate_item->certificate_data.dRepCredential);
                    txHashBuilder_addCertificate_dRepDeregistration(
                        &txHashBuilder,
                        &drepCred,
                        certificate_item->certificate_data.deposit
                    );
                    break;
                }
                case CERTIFICATE_DREP_UPDATE: {
                    ext_credential_t drepCred = _credentialForTxHash(&certificate_item->certificate_data.dRepCredential);
                    txHashBuilder_addCertificate_dRepUpdate(
                        &txHashBuilder,
                        &drepCred,
                        &certificate_item->certificate_data.anchor
                    );
                    break;
                }
                default:
                    // Pool registration not in scope
                    return send_error_and_reset(SWO_TX_PARSING_FAIL);
            }

            certificate_node = certificate_node->next;
        }
    }

    if (G_context.tx_info.transaction.num_withdrawals > 0) {
        txHashBuilder_enterWithdrawals(&txHashBuilder);
        s_flist_node *withdrawal_node = G_context.tx_info.transaction.withdrawals;
        while (withdrawal_node != NULL) {
            tx_withdrawal_list_item_t *withdrawal_item =
                (tx_withdrawal_list_item_t *) withdrawal_node;

            security_policy_t withdrawal_policy = policyForSignTxWithdrawal(
                G_context.tx_info.transaction.txSigningMode,
                &withdrawal_item->withdrawal_data.stakeCredential,
                &G_context.tx_info.warning_bits
            );

            switch (withdrawal_policy) {
                case POLICY_DENY:
                    TRACE("Withdrawal security policy denied");
                    return send_error_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
                case POLICY_SHOW:
                    plan->pair_count += 3;
                    break;
                case POLICY_HIDE:
                    break;
            }

            uint8_t reward_address[REWARD_ACCOUNT_LENGTH];
            size_t reward_addr_len = 0;

            switch (withdrawal_item->withdrawal_data.stakeCredential.type) {
                case EXT_CREDENTIAL_KEY_PATH:
                    reward_addr_len = constructRewardAddressFromKeyPath(
                        &withdrawal_item->withdrawal_data.stakeCredential.keyPath,
                        G_context.tx_info.transaction.networkId,
                        reward_address,
                        sizeof(reward_address)
                    );
                    break;
                case EXT_CREDENTIAL_KEY_HASH:
                    reward_addr_len = constructRewardAddressFromHash(
                        G_context.tx_info.transaction.networkId,
                        REWARD_HASH_SOURCE_KEY,
                        withdrawal_item->withdrawal_data.stakeCredential.keyHash,
                        ADDRESS_KEY_HASH_LENGTH,
                        reward_address,
                        sizeof(reward_address)
                    );
                    break;
                case EXT_CREDENTIAL_SCRIPT_HASH:
                    reward_addr_len = constructRewardAddressFromHash(
                        G_context.tx_info.transaction.networkId,
                        REWARD_HASH_SOURCE_SCRIPT,
                        withdrawal_item->withdrawal_data.stakeCredential.scriptHash,
                        SCRIPT_HASH_LENGTH,
                        reward_address,
                        sizeof(reward_address)
                    );
                    break;
                default:
                    return send_error_and_reset(SWO_TX_PARSING_FAIL);
            }

            if (reward_addr_len != REWARD_ACCOUNT_LENGTH) {
                return send_error_and_reset(SWO_TX_PARSING_FAIL);
            }

            txHashBuilder_addWithdrawal(&txHashBuilder,
                                       reward_address,
                                       reward_addr_len,
                                       withdrawal_item->withdrawal_data.amount);

            withdrawal_node = withdrawal_node->next;
        }
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
            mint_asset_group_list_item_t *mint_item =
                (mint_asset_group_list_item_t *) mint_node;

            txHashBuilder_addMint_tokenGroup(&txHashBuilder,
                                             mint_item->asset_group.policyId,
                                             MINTING_POLICY_ID_LENGTH,
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

    txHashBuilder_finalize(&txHashBuilder,
                          G_context.tx_info.tx_hash,
                          sizeof(G_context.tx_info.tx_hash));

    TRACE("Hash: %.*H", sizeof(G_context.tx_info.tx_hash), G_context.tx_info.tx_hash);

    // TODO: implement streaming review flow and remove this assertion once we're handling overflow.
    LEDGER_ASSERT(plan->pair_count <= UI_PAIR_LIMIT, "Need streaming UI fallback");
    LEDGER_ASSERT(plan->pair_count <= UINT8_MAX, "Pair count overflow");

    return SWO_SUCCESS;
}
