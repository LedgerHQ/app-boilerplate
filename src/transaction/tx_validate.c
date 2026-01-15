#include <stddef.h>  // NULL
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "tx_validate.h"

#include "os.h"
#include "cardano_tokens/cardano_tokens.h"
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
#include "utils/utils.h"
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
        bip44_pathToKeyHash(&credential->keyPath, result.keyHash, SIZEOF(result.keyHash));
    }

    return result;
}

/**
 * Convert ext_drep_t to a version suitable for tx hash building.
 * Converts KEY_PATH to KEY_HASH, leaves other types unchanged.
 * Does NOT modify the input drep.
 */
static drep_t _drepForTxHash(const ext_drep_t* ext_drep) {
    drep_t result = { .type = (drep_type_t) ext_drep->type };

    STATIC_ASSERT(SIZEOF(result.keyHash) == ADDRESS_KEY_HASH_LENGTH, "drep key hash size");
    STATIC_ASSERT(SIZEOF(result.scriptHash) == SCRIPT_HASH_LENGTH, "drep script hash size");

    switch (ext_drep->type) {
        case EXT_DREP_KEY_PATH:
            result.type = DREP_KEY_HASH;
            bip44_pathToKeyHash(&ext_drep->keyPath, result.keyHash, SIZEOF(result.keyHash));
            break;
        case EXT_DREP_KEY_HASH:
            LEDGER_ASSERT(ext_drep->keyHash != NULL, "NULL drep key hash pointer");
            result.type = DREP_KEY_HASH;
            memcpy(result.keyHash, ext_drep->keyHash, SIZEOF(result.keyHash));
            break;
        case EXT_DREP_SCRIPT_HASH:
            LEDGER_ASSERT(ext_drep->scriptHash != NULL, "NULL drep script hash pointer");
            result.type = DREP_SCRIPT_HASH;
            memcpy(result.scriptHash, ext_drep->scriptHash, SIZEOF(result.scriptHash));
            break;
        case EXT_DREP_ABSTAIN:
            result.type = DREP_ABSTAIN;
            break;
        case EXT_DREP_NO_CONFIDENCE:
            result.type = DREP_NO_CONFIDENCE;
            break;
        default:
            ASSERT(false);
    }

    return result;
}

static voter_t _voterForTxHash(const ext_voter_t* ext_voter) {
    voter_t voter = {0};

    STATIC_ASSERT(SIZEOF(voter.keyHash) == ADDRESS_KEY_HASH_LENGTH, "voter key hash size");
    STATIC_ASSERT(SIZEOF(voter.scriptHash) == SCRIPT_HASH_LENGTH, "voter script hash size");

    switch (ext_voter->type) {
        case EXT_VOTER_COMMITTEE_HOT_KEY_PATH:
            voter.type = VOTER_COMMITTEE_HOT_KEY_HASH;
            bip44_pathToKeyHash(&ext_voter->keyPath, voter.keyHash, SIZEOF(voter.keyHash));
            break;
        case EXT_VOTER_DREP_KEY_PATH:
            voter.type = VOTER_DREP_KEY_HASH;
            bip44_pathToKeyHash(&ext_voter->keyPath, voter.keyHash, SIZEOF(voter.keyHash));
            break;
        case EXT_VOTER_STAKE_POOL_KEY_PATH:
            voter.type = VOTER_STAKE_POOL_KEY_HASH;
            bip44_pathToKeyHash(&ext_voter->keyPath, voter.keyHash, SIZEOF(voter.keyHash));
            break;
        case EXT_VOTER_COMMITTEE_HOT_KEY_HASH:
            LEDGER_ASSERT(ext_voter->keyHash != NULL, "NULL committee hot key hash voter");
            voter.type = VOTER_COMMITTEE_HOT_KEY_HASH;
            memcpy(voter.keyHash, ext_voter->keyHash, SIZEOF(voter.keyHash));
            break;
        case EXT_VOTER_DREP_KEY_HASH:
            LEDGER_ASSERT(ext_voter->keyHash != NULL, "NULL drep key hash voter");
            voter.type = VOTER_DREP_KEY_HASH;
            memcpy(voter.keyHash, ext_voter->keyHash, SIZEOF(voter.keyHash));
            break;
        case EXT_VOTER_STAKE_POOL_KEY_HASH:
            LEDGER_ASSERT(ext_voter->keyHash != NULL, "NULL stake pool key hash voter");
            voter.type = VOTER_STAKE_POOL_KEY_HASH;
            memcpy(voter.keyHash, ext_voter->keyHash, SIZEOF(voter.keyHash));
            break;
        case EXT_VOTER_COMMITTEE_HOT_SCRIPT_HASH:
            LEDGER_ASSERT(ext_voter->scriptHash != NULL, "NULL committee hot script hash voter");
            voter.type = VOTER_COMMITTEE_HOT_SCRIPT_HASH;
            memcpy(voter.scriptHash, ext_voter->scriptHash, SIZEOF(voter.scriptHash));
            break;
        case EXT_VOTER_DREP_SCRIPT_HASH:
            LEDGER_ASSERT(ext_voter->scriptHash != NULL, "NULL drep script hash voter");
            voter.type = VOTER_DREP_SCRIPT_HASH;
            memcpy(voter.scriptHash, ext_voter->scriptHash, SIZEOF(voter.scriptHash));
            break;
        default:
            ASSERT(false);
    }

    return voter;
}

static int validate_and_hash_inputs(tx_hash_builder_t* txHashBuilder, tx_ui_plan_t* plan) {
    txHashBuilder_enterInputs(txHashBuilder);
    s_flist_node *node = G_context.tx_info.transaction.inputs;
    while (node != NULL) {
        tx_input_node_t *input_node = (tx_input_node_t *) node;
        const tx_input_t *input = &input_node->input;

        security_policy_t input_policy = policyForSignTxInput(
            G_context.tx_info.transaction.txSigningMode,
            input
        );

        switch (input_policy) {
            case POLICY_DENY:
                TRACE("Input security policy denied");
                return SWO_SECURITY_CONDITION_NOT_SATISFIED;
            case POLICY_SHOW:
                plan->pair_count += UI_PAIRS_INPUT;
                break;
            case POLICY_HIDE:
                break;
            default:
                LEDGER_ASSERT(false, "Unknown input policy");
        }

        txHashBuilder_addInput(txHashBuilder, input);
        node = node->next;
    }
    return SWO_SUCCESS;
}

// TODO clean up needed
static int validate_and_hash_outputs(tx_hash_builder_t* txHashBuilder, tx_ui_plan_t* plan) {
    txHashBuilder_enterOutputs(txHashBuilder);
    s_flist_node *node = G_context.tx_info.transaction.outputs;
    while (node != NULL) {
        tx_output_node_t *output_node = (tx_output_node_t *) node;
        tx_output_destination_storage_t *output_destination = &output_node->output_data.destination;
        const output_datum_t *output_datum = &output_node->output_data.datum;
        const ref_script_t *output_ref_script = &output_node->output_data.refScript;

        tx_output_description_t output_desc = {0};
        output_desc.format = output_node->output_data.format;
        output_desc.amount = output_node->output_data.adaAmount;
        output_desc.numAssetGroups = output_node->output_data.numAssetGroups;
        output_desc.includeDatum = output_datum->hasDatum;
        output_desc.includeRefScript = output_ref_script->hasRefScript;

        if (output_destination->type == DESTINATION_THIRD_PARTY) {
            output_desc.destination.type = DESTINATION_THIRD_PARTY;
            output_desc.destination.address.buffer = output_destination->address.buffer;
            output_desc.destination.address.size = output_destination->address.size;
        } else {
            output_desc.destination.type = DESTINATION_DEVICE_OWNED;
            output_desc.destination.params = &output_destination->params;
        }

        security_policy_t datum_policy = POLICY_HIDE;
        security_policy_t ref_script_policy = POLICY_HIDE;
        security_policy_t output_policy;
        if (output_destination->type == DESTINATION_THIRD_PARTY) {
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
                return SWO_SECURITY_CONDITION_NOT_SATISFIED;
            case POLICY_SHOW: {
                datum_policy = policyForSignTxOutputDatumHash(output_policy);
                ref_script_policy = policyForSignTxOutputRefScript(output_policy);

                plan->pair_count += UI_PAIRS_OUTPUT_BASE;
                if (output_destination->type == DESTINATION_DEVICE_OWNED) {
                    plan->pair_count += UI_PAIRS_OUTPUT_DEVICE_OWNED;
                }
                if (datum_policy == POLICY_SHOW && output_datum->hasDatum) {
                    plan->pair_count += UI_PAIRS_OUTPUT_DATUM;
                }
                if (ref_script_policy == POLICY_SHOW && output_ref_script->hasRefScript) {
                    plan->pair_count += UI_PAIRS_OUTPUT_REF_SCRIPT;
                }

                if (output_node->output_data.assetGroups != NULL) {
                    uint16_t asset_group_count = 0;
                    s_flist_node *node2 = output_node->output_data.assetGroups;
                    while (node2 != NULL) {
                        output_asset_group_node_t *asset_group_node =
                            (output_asset_group_node_t *) node2;
                        const output_asset_group_t *asset_group = &asset_group_node->asset_group;
                        {
                            uint16_t token_count = 0;
                            s_flist_node *node3 = asset_group->tokens;
                            while (node3 != NULL) {
                                plan->pair_count += UI_PAIRS_TOKEN;
                                token_count++;
                                node3 = node3->next;
                            }
                            LEDGER_ASSERT(token_count == asset_group->numTokens,
                                          "Output asset group token count mismatch");
                        }
                        asset_group_count++;
                        node2 = node2->next;
                    }
                    LEDGER_ASSERT(asset_group_count == output_node->output_data.numAssetGroups,
                                  "Output asset group count mismatch");
                }
                break;
            }
            case POLICY_HIDE:
                datum_policy = policyForSignTxOutputDatumHash(output_policy);
                ref_script_policy = policyForSignTxOutputRefScript(output_policy);
                break;
            default:
                LEDGER_ASSERT(false, "Unknown output policy");
                break;
        }

        if (output_destination->type == DESTINATION_THIRD_PARTY) {
            txHashBuilder_addOutput_topLevelData(txHashBuilder, &output_desc);
        } else {
            uint8_t *address_bytes = (uint8_t *) app_mem_alloc(MAX_ADDRESS_LENGTH);
            if (address_bytes == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }

            size_t address_size = deriveAddress(
                &output_destination->params,
                address_bytes,
                MAX_ADDRESS_LENGTH
            );

            if (address_size == 0 || address_size > MAX_ADDRESS_LENGTH) {
                app_mem_free(address_bytes);
                return SWO_INCORRECT_DATA;
            }

            output_desc.destination.type = DESTINATION_THIRD_PARTY;
            output_desc.destination.address.buffer = address_bytes;
            output_desc.destination.address.size = address_size;
            txHashBuilder_addOutput_topLevelData(txHashBuilder, &output_desc);
            app_mem_free(address_bytes);
        }

        {
            uint16_t asset_group_count = 0;
            s_flist_node *node2 = output_node->output_data.assetGroups;
            while (node2 != NULL) {
                output_asset_group_node_t *asset_group_node =
                    (output_asset_group_node_t *) node2;
                const output_asset_group_t *asset_group = &asset_group_node->asset_group;
                txHashBuilder_addOutput_tokenGroup(txHashBuilder,
                                                   asset_group->policyId,
                                                   MINTING_POLICY_ID_LENGTH,
                                                   asset_group->numTokens);

                s_flist_node *node3 = asset_group->tokens;
                while (node3 != NULL) {
                    output_token_node_t *token_node = (output_token_node_t *) node3;
                    const output_token_t *token = &token_node->token_data;
                    txHashBuilder_addOutput_token(txHashBuilder,
                                                    token->assetName,
                                                    token->assetNameLen,
                                                    (uint64_t) token->amount);
                    node3 = node3->next;
                }

                asset_group_count++;
                node2 = node2->next;
            }
            LEDGER_ASSERT(asset_group_count == output_node->output_data.numAssetGroups,
                          "Output asset group count mismatch");
        }

        if (output_datum->hasDatum) {
            if (output_datum->type == DATUM_HASH) {
                txHashBuilder_addOutput_datum(txHashBuilder,
                                             DATUM_HASH,
                                             output_datum->hash,
                                             OUTPUT_DATUM_HASH_LENGTH);
            } else {
                txHashBuilder_addOutput_datum(txHashBuilder,
                                             DATUM_INLINE,
                                             output_datum->inline_data.data,
                                             output_datum->inline_data.size);
                txHashBuilder_addOutput_datum_inline_chunk(txHashBuilder,
                                                           output_datum->inline_data.data,
                                                           output_datum->inline_data.size);
            }
        }

        if (output_ref_script->hasRefScript) {
            txHashBuilder_addOutput_referenceScript(txHashBuilder,
                                                   output_ref_script->size);
            txHashBuilder_addOutput_referenceScript_dataChunk(txHashBuilder,
                                                             output_ref_script->data,
                                                             output_ref_script->size);
        }

        node = node->next;
    }
    return SWO_SUCCESS;
}

static int validate_and_hash_fee(tx_hash_builder_t* txHashBuilder, tx_ui_plan_t* plan) {
    security_policy_t fee_policy = policyForSignTxFee(
        G_context.tx_info.transaction.txSigningMode,
        G_context.tx_info.transaction.fee,
        &G_context.tx_info.warning_bits
    );

    switch (fee_policy) {
        case POLICY_DENY:
            return SWO_SECURITY_CONDITION_NOT_SATISFIED;
        case POLICY_SHOW:
            plan->pair_count += UI_PAIRS_FEE;
            break;
        case POLICY_HIDE:
            break;
        default:
            LEDGER_ASSERT(false, "Unknown fee policy");
            break;
    }

    txHashBuilder_addFee(txHashBuilder, G_context.tx_info.transaction.fee);
    return SWO_SUCCESS;
}

static int validate_and_hash_ttl(tx_hash_builder_t* txHashBuilder, tx_ui_plan_t* plan) {
    if (!G_context.tx_info.transaction.includeTtl) {
        return SWO_SUCCESS;
    }

    security_policy_t ttl_policy = policyForSignTxTtl(G_context.tx_info.transaction.ttl);
    switch (ttl_policy) {
        case POLICY_DENY:
            return SWO_SECURITY_CONDITION_NOT_SATISFIED;
        case POLICY_SHOW:
            plan->pair_count += UI_PAIRS_TTL;
            break;
        case POLICY_HIDE:
            break;
        default:
            LEDGER_ASSERT(false, "Unknown ttl policy");
            break;
    }

    txHashBuilder_addTtl(txHashBuilder, G_context.tx_info.transaction.ttl);
    return SWO_SUCCESS;
}

static int validate_and_hash_certificates(tx_hash_builder_t* txHashBuilder, tx_ui_plan_t* plan) {
    if (G_context.tx_info.transaction.num_certificates == 0) {
        return SWO_SUCCESS;
    }

    txHashBuilder_enterCertificates(txHashBuilder);
    s_flist_node *node = G_context.tx_info.transaction.certificates;
    while (node != NULL) {
        tx_certificate_node_t *certificate_node = (tx_certificate_node_t *) node;
        const certificate_data_t *certificate = &certificate_node->certificate;

        security_policy_t generic_policy = policyForSignTxCertificate(
            G_context.tx_info.transaction.txSigningMode,
            certificate->type
        );
        switch (generic_policy) {
            case POLICY_DENY:
                return SWO_SECURITY_CONDITION_NOT_SATISFIED;
            case POLICY_SHOW:
            case POLICY_HIDE:
                break;
            default:
                LEDGER_ASSERT(false, "Unknown certificate policy");
                break;
        }

        security_policy_t cert_policy = POLICY_HIDE;
        switch (certificate->type) {
            case CERTIFICATE_STAKE_REGISTRATION:
            case CERTIFICATE_STAKE_DEREGISTRATION:
            case CERTIFICATE_STAKE_REGISTRATION_CONWAY:
            case CERTIFICATE_STAKE_DEREGISTRATION_CONWAY: {
                cert_policy = policyForSignTxCertificateStaking(
                    G_context.tx_info.transaction.txSigningMode,
                    certificate->type,
                    &certificate->stakeCredential
                );
                switch (cert_policy) {
                    case POLICY_DENY:
                        return SWO_SECURITY_CONDITION_NOT_SATISFIED;
                    case POLICY_SHOW:
                        switch (certificate->type) {
                            case CERTIFICATE_STAKE_REGISTRATION:
                                plan->pair_count += UI_PAIRS_CERTIFICATE_STAKE_REGISTRATION;
                                break;
                            case CERTIFICATE_STAKE_DEREGISTRATION:
                                plan->pair_count += UI_PAIRS_CERTIFICATE_STAKE_DEREGISTRATION;
                                break;
                            case CERTIFICATE_STAKE_REGISTRATION_CONWAY:
                                plan->pair_count += UI_PAIRS_CERTIFICATE_STAKE_REGISTRATION_CONWAY;
                                break;
                            case CERTIFICATE_STAKE_DEREGISTRATION_CONWAY:
                                plan->pair_count += UI_PAIRS_CERTIFICATE_STAKE_DEREGISTRATION_CONWAY;
                                break;
                            default:
                                LEDGER_ASSERT(false, "Unknown staking certificate type");
                                break;
                        }
                        break;
                    case POLICY_HIDE:
                        break;
                    default:
                        LEDGER_ASSERT(false, "Unknown staking certificate policy");
                        break;
                }
                break;
            }
            case CERTIFICATE_STAKE_DELEGATION: {
                cert_policy = policyForSignTxCertificateStaking(
                    G_context.tx_info.transaction.txSigningMode,
                    certificate->type,
                    &certificate->stakeCredential
                );
                switch (cert_policy) {
                    case POLICY_DENY:
                        return SWO_SECURITY_CONDITION_NOT_SATISFIED;
                    case POLICY_SHOW:
                        plan->pair_count += UI_PAIRS_CERTIFICATE_STAKE_DELEGATION;
                        break;
                    case POLICY_HIDE:
                        break;
                    default:
                        LEDGER_ASSERT(false, "Unknown stake delegation policy");
                        break;
                }
                break;
            }
            case CERTIFICATE_VOTE_DELEGATION: {
                cert_policy = policyForSignTxCertificateVoteDelegation(
                    G_context.tx_info.transaction.txSigningMode,
                    &certificate->stakeCredential,
                    &certificate->drep
                );
                switch (cert_policy) {
                    case POLICY_DENY:
                        return SWO_SECURITY_CONDITION_NOT_SATISFIED;
                    case POLICY_SHOW:
                        plan->pair_count += UI_PAIRS_CERTIFICATE_VOTE_DELEGATION;
                        break;
                    case POLICY_HIDE:
                        break;
                    default:
                        LEDGER_ASSERT(false, "Unknown vote delegation policy");
                        break;
                }
                break;
            }
            case CERTIFICATE_AUTHORIZE_COMMITTEE_HOT: {
                cert_policy = policyForSignTxCertificateCommitteeAuth(
                    G_context.tx_info.transaction.txSigningMode,
                    &certificate->coldCredential,
                    &certificate->hotCredential
                );
                switch (cert_policy) {
                    case POLICY_DENY:
                        return SWO_SECURITY_CONDITION_NOT_SATISFIED;
                    case POLICY_SHOW:
                        plan->pair_count += UI_PAIRS_CERTIFICATE_AUTHORIZE_COMMITTEE_HOT;
                        break;
                    case POLICY_HIDE:
                        break;
                    default:
                        LEDGER_ASSERT(false, "Unknown committee auth policy");
                        break;
                }
                break;
            }
            case CERTIFICATE_RESIGN_COMMITTEE_COLD: {
                cert_policy = policyForSignTxCertificateCommitteeResign(
                    G_context.tx_info.transaction.txSigningMode,
                    &certificate->coldCredential
                );
                switch (cert_policy) {
                    case POLICY_DENY:
                        return SWO_SECURITY_CONDITION_NOT_SATISFIED;
                    case POLICY_SHOW:
                        plan->pair_count += UI_PAIRS_CERTIFICATE_RESIGN_COMMITTEE_COLD;
                        if (certificate->anchor.isIncluded) {
                            plan->pair_count += UI_PAIRS_ANCHOR;
                        }
                        break;
                    case POLICY_HIDE:
                        break;
                    default:
                        LEDGER_ASSERT(false, "Unknown committee resign policy");
                        break;
                }
                break;
            }
            case CERTIFICATE_DREP_REGISTRATION:
            case CERTIFICATE_DREP_DEREGISTRATION:
            case CERTIFICATE_DREP_UPDATE: {
                cert_policy = policyForSignTxCertificateDRep(
                    G_context.tx_info.transaction.txSigningMode,
                    &certificate->dRepCredential
                );
                switch (cert_policy) {
                    case POLICY_DENY:
                        return SWO_SECURITY_CONDITION_NOT_SATISFIED;
                    case POLICY_SHOW:
                        if (certificate->type == CERTIFICATE_DREP_REGISTRATION) {
                            plan->pair_count += UI_PAIRS_CERTIFICATE_DREP_REGISTRATION;
                            if (certificate->anchor.isIncluded) {
                                plan->pair_count += UI_PAIRS_ANCHOR;
                            }
                        } else if (certificate->type == CERTIFICATE_DREP_DEREGISTRATION) {
                            plan->pair_count += UI_PAIRS_CERTIFICATE_DREP_DEREGISTRATION;
                        } else {
                            plan->pair_count += UI_PAIRS_CERTIFICATE_DREP_UPDATE;
                            if (certificate->anchor.isIncluded) {
                                plan->pair_count += UI_PAIRS_ANCHOR;
                            }
                        }
                        break;
                    case POLICY_HIDE:
                        break;
                    default:
                        LEDGER_ASSERT(false, "Unknown drep certificate policy");
                        break;
                }
                break;
            }
            case CERTIFICATE_STAKE_POOL_REGISTRATION: {
                pool_owner_counts_t owner_counts =
                    count_pool_owner_nodes(certificate->poolRegistration.poolOwners);
                if (G_context.tx_info.transaction.txSigningMode ==
                    SIGN_TX_SIGNINGMODE_POOL_REGISTRATION_OWNER) {
                    LEDGER_ASSERT(!G_context.tx_info.pool_owner_path_present,
                                  "Multiple pool registrations in owner mode");
                    if (owner_counts.path_owners == 1) {
                        s_flist_node *node2 = certificate->poolRegistration.poolOwners;
                        while (node2 != NULL) {
                            tx_certificate_node_t *owner_node = (tx_certificate_node_t *) node2;
                            const ext_credential_t *owner_credential =
                                &owner_node->certificate.stakeCredential;
                            if (owner_credential->type == EXT_CREDENTIAL_KEY_PATH) {
                                G_context.tx_info.pool_owner_path = owner_credential->keyPath;
                                G_context.tx_info.pool_owner_path_present = true;
                                break;
                            }
                            node2 = node2->next;
                        }
                        LEDGER_ASSERT(G_context.tx_info.pool_owner_path_present,
                                      "Pool owner path missing");
                    }
                }
                cert_policy = policyForSignTxStakePoolRegistrationInit(
                    G_context.tx_info.transaction.txSigningMode,
                    certificate->poolRegistration.numPoolOwners,
                    owner_counts.path_owners
                );
                switch (cert_policy) {
                    case POLICY_DENY:
                        return SWO_SECURITY_CONDITION_NOT_SATISFIED;
                    case POLICY_HIDE:
                        break;
                    case POLICY_SHOW: {
                        // Calculate UI pairs for pool registration certificate display
                        // Each policy decision determines which fields are shown to the user
                        uint16_t pool_pairs = UI_PAIRS_CERTIFICATE_POOL_REGISTRATION_BASE;

                        // Pool ID (bech32 encoded pool keyhash)
                        security_policy_t pool_id_policy = policyForSignTxStakePoolRegistrationPoolId(
                            G_context.tx_info.transaction.txSigningMode,
                            &certificate->poolId
                        );
                        switch (pool_id_policy) {
                            case POLICY_DENY:
                                return SWO_SECURITY_CONDITION_NOT_SATISFIED;
                            case POLICY_SHOW:
                                pool_pairs += UI_PAIRS_POOL_ID;
                                break;
                            case POLICY_HIDE:
                                break;
                            default:
                                LEDGER_ASSERT(false, "Unknown pool id policy");
                                break;
                        }

                        // VRF Key Hash (verification key for pool's VRF)
                        security_policy_t vrf_policy = policyForSignTxStakePoolRegistrationVrfKey(
                            G_context.tx_info.transaction.txSigningMode
                        );
                        switch (vrf_policy) {
                            case POLICY_DENY:
                                return SWO_SECURITY_CONDITION_NOT_SATISFIED;
                            case POLICY_SHOW:
                                pool_pairs += UI_PAIRS_POOL_VRF_KEY;
                                break;
                            case POLICY_HIDE:
                                break;
                            default:
                                LEDGER_ASSERT(false, "Unknown vrf policy");
                                break;
                        }

                        // Fixed pool fields: pledge (lovelace), cost (lovelace), margin (fraction)
                        pool_pairs += UI_PAIRS_POOL_FIXED;

                        // Reward Account (where pool rewards are distributed to)
                        security_policy_t reward_policy = policyForSignTxStakePoolRegistrationRewardAccount(
                            G_context.tx_info.transaction.txSigningMode,
                            G_context.tx_info.transaction.networkId,
                            &certificate->poolRegistration.rewardAccount
                        );
                        switch (reward_policy) {
                            case POLICY_DENY:
                                return SWO_SECURITY_CONDITION_NOT_SATISFIED;
                            case POLICY_SHOW:
                                pool_pairs += UI_PAIRS_POOL_REWARD_ACCOUNT;
                                break;
                            case POLICY_HIDE:
                                break;
                            default:
                                LEDGER_ASSERT(false, "Unknown reward account policy");
                                break;
                        }

                        // Pool Owners (variable count: each owner gets 1 pair if SHOW policy)
                        {
                            s_flist_node *node2 = certificate->poolRegistration.poolOwners;
                            while (node2 != NULL) {
                                tx_certificate_node_t *owner_node = (tx_certificate_node_t *) node2;
                                ext_credential_t *owner_credential =
                                    &owner_node->certificate.stakeCredential;

                                security_policy_t owner_policy = policyForSignTxStakePoolRegistrationOwner(
                                    G_context.tx_info.transaction.txSigningMode,
                                    owner_credential
                                );
                                switch (owner_policy) {
                                    case POLICY_DENY:
                                        return SWO_SECURITY_CONDITION_NOT_SATISFIED;
                                    case POLICY_SHOW:
                                        pool_pairs += UI_PAIRS_POOL_OWNER;
                                        break;
                                    case POLICY_HIDE:
                                        break;
                                    default:
                                        LEDGER_ASSERT(false, "Unknown owner policy");
                                        break;
                                }

                                node2 = node2->next;
                            }
                        }
                        ASSERT(owner_counts.total_owners ==
                               certificate->poolRegistration.numPoolOwners);
                        // If no owners present, add placeholder pair to indicate "no owners"
                        if (owner_counts.total_owners == 0) {
                            pool_pairs += UI_PAIRS_POOL_NO_OWNERS;
                        }

                        // Pool Relays (variable count: each relay can have multiple fields)
                        // Relay types: single host IPv4/IPv6, single host by DNS, multi-host by DNS
                        // Each relay itself is 1 pair, plus additional pairs for its endpoints
                        uint32_t relay_count = 0;
                        {
                            s_flist_node *node2 = certificate->poolRegistration.relays;
                            while (node2 != NULL) {
                                tx_certificate_node_t *relay_node = (tx_certificate_node_t *) node2;
                                const pool_relay_t *relay = (const pool_relay_t *) &relay_node->certificate;

                                security_policy_t relay_policy = policyForSignTxStakePoolRegistrationRelay(
                                    G_context.tx_info.transaction.txSigningMode,
                                    relay
                                );
                                switch (relay_policy) {
                                    case POLICY_DENY:
                                        return SWO_SECURITY_CONDITION_NOT_SATISFIED;
                                    case POLICY_HIDE:
                                        break;
                                    case POLICY_SHOW:
                                        pool_pairs += UI_PAIRS_POOL_RELAY_HEADER;
                                        switch (relay->format) {
                                            case RELAY_SINGLE_HOST_IP:
                                                // Single host IP relay: can have IPv4, IPv6, and port
                                                if (!relay->ipv4.isNull) {
                                                    pool_pairs += UI_PAIRS_POOL_RELAY_IPV4;
                                                }
                                                if (!relay->ipv6.isNull) {
                                                    pool_pairs += UI_PAIRS_POOL_RELAY_IPV6;
                                                }
                                                if (!relay->port.isNull) {
                                                    pool_pairs += UI_PAIRS_POOL_RELAY_PORT;
                                                }
                                                break;
                                            case RELAY_SINGLE_HOST_NAME:
                                                // Single host DNS relay: can have DNS name and port
                                                if (relay->dnsNameSize > 0) {
                                                    pool_pairs += UI_PAIRS_POOL_RELAY_DNS;
                                                }
                                                if (!relay->port.isNull) {
                                                    pool_pairs += UI_PAIRS_POOL_RELAY_PORT;
                                                }
                                                break;
                                            case RELAY_MULTIPLE_HOST_NAME:
                                                // Multi-host DNS relay: only DNS name (no port)
                                                if (relay->dnsNameSize > 0) {
                                                    pool_pairs += UI_PAIRS_POOL_RELAY_DNS;
                                                }
                                                break;
                                            default:
                                                LEDGER_ASSERT(false, "Unknown relay format type");
                                        }
                                        break;
                                    default:
                                        LEDGER_ASSERT(false, "Unknown relay policy");
                                        break;
                                }

                                node2 = node2->next;
                                relay_count++;
                            }
                        }
                        ASSERT(relay_count == certificate->poolRegistration.numRelays);
                        // If no relays present, add placeholder pair to indicate "no relays"
                        if (relay_count == 0) {
                            pool_pairs += UI_PAIRS_POOL_NO_RELAYS;
                        }

                        // Pool Metadata (optional: URL and hash)
                        if (certificate->poolRegistration.poolMetadataIsNull) {
                            // No metadata case
                            security_policy_t no_metadata_policy =
                                policyForSignTxStakePoolRegistrationNoMetadata();
                            switch (no_metadata_policy) {
                                case POLICY_DENY:
                                    return SWO_SECURITY_CONDITION_NOT_SATISFIED;
                                case POLICY_SHOW:
                                    pool_pairs += UI_PAIRS_POOL_NO_METADATA;
                                    break;
                                case POLICY_HIDE:
                                    break;
                                default:
                                    LEDGER_ASSERT(false, "Unknown metadata policy");
                                    break;
                            }
                        } else {
                            // Metadata present case: URL + hash
                            security_policy_t metadata_policy =
                                policyForSignTxStakePoolRegistrationMetadata();
                            switch (metadata_policy) {
                                case POLICY_DENY:
                                    return SWO_SECURITY_CONDITION_NOT_SATISFIED;
                                case POLICY_SHOW:
                                    pool_pairs += UI_PAIRS_POOL_METADATA;
                                    break;
                                case POLICY_HIDE:
                                    break;
                                default:
                                    LEDGER_ASSERT(false, "Unknown metadata policy");
                                    break;
                            }
                        }

                        plan->pair_count += pool_pairs;
                        break;
                    }
                    default:
                        LEDGER_ASSERT(false, "Unknown pool registration policy");
                        break;
                }
                break;
            }
            case CERTIFICATE_STAKE_POOL_RETIREMENT: {
                cert_policy = policyForSignTxCertificateStakePoolRetirement(
                    G_context.tx_info.transaction.txSigningMode,
                    &certificate->poolCredential,
                    certificate->retirementEpoch
                );
                switch (cert_policy) {
                    case POLICY_DENY:
                        return SWO_SECURITY_CONDITION_NOT_SATISFIED;
                    case POLICY_SHOW:
                        plan->pair_count += UI_PAIRS_CERTIFICATE_POOL_RETIREMENT;
                        break;
                    case POLICY_HIDE:
                        break;
                    default:
                        LEDGER_ASSERT(false, "Unknown pool retirement policy");
                        break;
                }
                break;
            }
            default:
                LEDGER_ASSERT(false, "Unknown certificate type");
                break;
        }

        switch (certificate->type) {
            case CERTIFICATE_STAKE_REGISTRATION:
            case CERTIFICATE_STAKE_DEREGISTRATION: {
                ext_credential_t stakeCred = _credentialForTxHash(&certificate->stakeCredential);
                txHashBuilder_addCertificate_stakingOld(
                    txHashBuilder,
                    certificate->type,
                    &stakeCred
                );
                break;
            }
            case CERTIFICATE_STAKE_DELEGATION: {
                ext_credential_t stakeCred = _credentialForTxHash(&certificate->stakeCredential);
                txHashBuilder_addCertificate_stakeDelegation(
                    txHashBuilder,
                    &stakeCred,
                    certificate->poolKeyHash,
                    POOL_KEY_HASH_LENGTH
                );
                break;
            }
            case CERTIFICATE_STAKE_REGISTRATION_CONWAY:
            case CERTIFICATE_STAKE_DEREGISTRATION_CONWAY: {
                ext_credential_t stakeCred = _credentialForTxHash(&certificate->stakeCredential);
                txHashBuilder_addCertificate_staking(
                    txHashBuilder,
                    certificate->type,
                    &stakeCred,
                    certificate->deposit
                );
                break;
            }
            case CERTIFICATE_STAKE_POOL_RETIREMENT: {
                const ext_credential_t *poolCred = &certificate->poolCredential;
                uint8_t poolKeyHash[POOL_KEY_HASH_LENGTH];
                TRACE("Pool retirement credential type = %d", poolCred->type);
                switch (poolCred->type) {
                    case EXT_CREDENTIAL_KEY_PATH:
                        TRACE("Pool retirement key path length = %u", poolCred->keyPath.length);
                        bip44_pathToKeyHash(&poolCred->keyPath, poolKeyHash, sizeof(poolKeyHash));
                        break;
                    case EXT_CREDENTIAL_KEY_HASH:
                        TRACE("Pool retirement credential key hash first byte = %02x", poolCred->keyHash[0]);
                        STATIC_ASSERT(ADDRESS_KEY_HASH_LENGTH == POOL_KEY_HASH_LENGTH,
                                      "pool credential hash size mismatch");
                        memcpy(poolKeyHash, poolCred->keyHash, POOL_KEY_HASH_LENGTH);
                        break;
                    default:
                        LEDGER_ASSERT(false, "Unsupported pool credential type for retirement");
                        break;
                }
                TRACE("Derived pool key hash first byte = %02x", poolKeyHash[0]);
                txHashBuilder_addCertificate_poolRetirement(
                    txHashBuilder,
                    poolKeyHash,
                    POOL_KEY_HASH_LENGTH,
                    certificate->retirementEpoch
                );
                break;
            }
            case CERTIFICATE_STAKE_POOL_REGISTRATION: {
                const pool_registration_data_t *poolReg = &certificate->poolRegistration;

                txHashBuilder_poolRegistrationCertificate_enter(
                    txHashBuilder,
                    poolReg->numPoolOwners,
                    poolReg->numRelays
                );

                uint8_t poolKeyHash[POOL_KEY_HASH_LENGTH];
                if (certificate->poolId.keyReferenceType == KEY_REFERENCE_PATH) {
                    bip44_pathToKeyHash(&certificate->poolId.path, poolKeyHash, sizeof(poolKeyHash));
                } else {
                    LEDGER_ASSERT(certificate->poolId.hash != NULL, "NULL pool ID hash");
                    memcpy(poolKeyHash, certificate->poolId.hash, POOL_KEY_HASH_LENGTH);
                }
                txHashBuilder_poolRegistrationCertificate_poolKeyHash(
                    txHashBuilder,
                    poolKeyHash,
                    sizeof(poolKeyHash)
                );

                txHashBuilder_poolRegistrationCertificate_vrfKeyHash(
                    txHashBuilder,
                    certificate->vrfKeyHash,
                    VRF_KEY_HASH_LENGTH
                );

                txHashBuilder_poolRegistrationCertificate_financials(
                    txHashBuilder,
                    poolReg->pledge,
                    poolReg->cost,
                    poolReg->marginNumerator,
                    poolReg->marginDenominator
                );

                uint8_t rewardAccountBuf[REWARD_ACCOUNT_LENGTH];
                poolRewardAccountToBuffer(
                    &poolReg->rewardAccount,
                    G_context.tx_info.transaction.networkId,
                    rewardAccountBuf
                );
                txHashBuilder_poolRegistrationCertificate_rewardAccount(
                    txHashBuilder,
                    rewardAccountBuf,
                    sizeof(rewardAccountBuf)
                );

                txHashBuilder_addPoolRegistrationCertificate_enterOwners(txHashBuilder);
                {
                    s_flist_node *node2 = poolReg->poolOwners;
                    while (node2 != NULL) {
                        tx_certificate_node_t *owner_node = (tx_certificate_node_t *) node2;
                        ext_credential_t *owner_credential = &owner_node->certificate.stakeCredential;
                        ext_credential_t owner_credential_for_hash =
                            _credentialForTxHash(owner_credential);
                        txHashBuilder_addPoolRegistrationCertificate_addOwner(
                            txHashBuilder,
                            owner_credential_for_hash.keyHash,
                            sizeof(owner_credential_for_hash.keyHash)
                        );
                        node2 = node2->next;
                    }
                }

                txHashBuilder_addPoolRegistrationCertificate_enterRelays(txHashBuilder);
                {
                    s_flist_node *node2 = poolReg->relays;
                    while (node2 != NULL) {
                        tx_certificate_node_t *relay_node = (tx_certificate_node_t *) node2;
                        const pool_relay_t *relay = (const pool_relay_t *) &relay_node->certificate;
                        txHashBuilder_addPoolRegistrationCertificate_addRelay(txHashBuilder, relay);
                        node2 = node2->next;
                    }
                }

                if (poolReg->poolMetadataIsNull) {
                    txHashBuilder_addPoolRegistrationCertificate_addPoolMetadata_null(txHashBuilder);
                } else {
                    txHashBuilder_addPoolRegistrationCertificate_addPoolMetadata(
                        txHashBuilder,
                        poolReg->poolMetadata.url,
                        poolReg->poolMetadata.urlSize,
                        poolReg->poolMetadata.hash,
                        POOL_METADATA_HASH_LENGTH
                    );
                }
                break;
            }
            case CERTIFICATE_VOTE_DELEGATION: {
                ext_credential_t stakeCred = _credentialForTxHash(&certificate->stakeCredential);
                drep_t drep = _drepForTxHash(&certificate->drep);
                txHashBuilder_addCertificate_voteDelegation(
                    txHashBuilder,
                    &stakeCred,
                    &drep
                );
                break;
            }
            case CERTIFICATE_AUTHORIZE_COMMITTEE_HOT: {
                ext_credential_t coldCred = _credentialForTxHash(&certificate->coldCredential);
                ext_credential_t hotCred = _credentialForTxHash(&certificate->hotCredential);
                txHashBuilder_addCertificate_committeeAuthHot(
                    txHashBuilder,
                    &coldCred,
                    &hotCred
                );
                break;
            }
            case CERTIFICATE_RESIGN_COMMITTEE_COLD: {
                ext_credential_t coldCred = _credentialForTxHash(&certificate->coldCredential);
                txHashBuilder_addCertificate_committeeResign(
                    txHashBuilder,
                    &coldCred,
                    &certificate->anchor
                );
                break;
            }
            case CERTIFICATE_DREP_REGISTRATION: {
                ext_credential_t drepCred = _credentialForTxHash(&certificate->dRepCredential);
                txHashBuilder_addCertificate_dRepRegistration(
                    txHashBuilder,
                    &drepCred,
                    certificate->deposit,
                    &certificate->anchor
                );
                break;
            }
            case CERTIFICATE_DREP_DEREGISTRATION: {
                ext_credential_t drepCred = _credentialForTxHash(&certificate->dRepCredential);
                txHashBuilder_addCertificate_dRepDeregistration(
                    txHashBuilder,
                    &drepCred,
                    certificate->deposit
                );
                break;
            }
            case CERTIFICATE_DREP_UPDATE: {
                ext_credential_t drepCred = _credentialForTxHash(&certificate->dRepCredential);
                txHashBuilder_addCertificate_dRepUpdate(
                    txHashBuilder,
                    &drepCred,
                    &certificate->anchor
                );
                break;
            }
            default:
                LEDGER_ASSERT(false, "Unsupported certificate type in tx_prepare");
                break;
        }

        node = node->next;
    }

    return SWO_SUCCESS;
}

static int validate_and_hash_withdrawals(tx_hash_builder_t* txHashBuilder, tx_ui_plan_t* plan) {
    if (G_context.tx_info.transaction.num_withdrawals == 0) {
        return SWO_SUCCESS;
    }

    txHashBuilder_enterWithdrawals(txHashBuilder);

    uint8_t previousRewardAccount[REWARD_ACCOUNT_LENGTH];
    explicit_bzero(previousRewardAccount, REWARD_ACCOUNT_LENGTH);
    bool isFirstWithdrawal = true;

    s_flist_node *node = G_context.tx_info.transaction.withdrawals;
    while (node != NULL) {
        tx_withdrawal_node_t *withdrawal_node = (tx_withdrawal_node_t *) node;
        const withdrawal_t *withdrawal = &withdrawal_node->withdrawal;

        security_policy_t withdrawal_policy = policyForSignTxWithdrawal(
            G_context.tx_info.transaction.txSigningMode,
            &withdrawal->stakeCredential,
            &G_context.tx_info.warning_bits
        );

        switch (withdrawal_policy) {
            case POLICY_DENY:
                TRACE("Withdrawal security policy denied");
                return SWO_SECURITY_CONDITION_NOT_SATISFIED;
            case POLICY_SHOW:
                if (withdrawal->stakeCredential.type == EXT_CREDENTIAL_KEY_PATH) {
                    plan->pair_count += UI_PAIRS_WITHDRAWAL_KEY_PATH;
                } else {
                    plan->pair_count += UI_PAIRS_WITHDRAWAL_OTHER;
                }
                break;
            case POLICY_HIDE:
                break;
            default:
                LEDGER_ASSERT(false, "Unknown withdrawal policy");
                break;
        }

        uint8_t reward_address[REWARD_ACCOUNT_LENGTH];
        size_t reward_addr_len = 0;
        switch (withdrawal->stakeCredential.type) {
            case EXT_CREDENTIAL_KEY_PATH:
                reward_addr_len = constructRewardAddressFromKeyPath(
                    &withdrawal->stakeCredential.keyPath,
                    G_context.tx_info.transaction.networkId,
                    reward_address,
                    sizeof(reward_address)
                );
                break;
            case EXT_CREDENTIAL_KEY_HASH:
                reward_addr_len = constructRewardAddressFromHash(
                    G_context.tx_info.transaction.networkId,
                    REWARD_HASH_SOURCE_KEY,
                    withdrawal->stakeCredential.keyHash,
                    ADDRESS_KEY_HASH_LENGTH,
                    reward_address,
                    sizeof(reward_address)
                );
                break;
            case EXT_CREDENTIAL_SCRIPT_HASH:
                reward_addr_len = constructRewardAddressFromHash(
                    G_context.tx_info.transaction.networkId,
                    REWARD_HASH_SOURCE_SCRIPT,
                    withdrawal->stakeCredential.scriptHash,
                    SCRIPT_HASH_LENGTH,
                    reward_address,
                    sizeof(reward_address)
                );
                break;
            default:
                LEDGER_ASSERT(false, "Unknown withdrawal credential type");
                break;
        }

        LEDGER_ASSERT(reward_addr_len == REWARD_ACCOUNT_LENGTH, "Invalid reward address length");

        if (!isFirstWithdrawal) {
            if (!cbor_mapKeyFulfillsCanonicalOrdering(
                    previousRewardAccount,
                    REWARD_ACCOUNT_LENGTH,
                    reward_address,
                    reward_addr_len)) {
                TRACE("Withdrawals not in canonical order");
                return SWO_TX_PARSING_FAIL_WITHDRAWALS;
            }
        }

        memmove(previousRewardAccount, reward_address, reward_addr_len);
        isFirstWithdrawal = false;

        txHashBuilder_addWithdrawal(txHashBuilder,
                                   reward_address,
                                   reward_addr_len,
                                   withdrawal->amount);

        node = node->next;
    }

    return SWO_SUCCESS;
}

static int validate_and_hash_aux_data_hash(tx_hash_builder_t* txHashBuilder, tx_ui_plan_t* plan) {
    if (!G_context.tx_info.transaction.includeAuxDataHash) {
        return SWO_SUCCESS;
    }

    security_policy_t aux_policy =
        policyForSignTxAuxData(G_context.tx_info.transaction.auxDataType);
    switch (aux_policy) {
        case POLICY_DENY:
            return SWO_SECURITY_CONDITION_NOT_SATISFIED;
        case POLICY_SHOW:
            plan->pair_count += UI_PAIRS_AUXILIARY_DATA_HASH;
            break;
        case POLICY_HIDE:
            break;
        default:
            LEDGER_ASSERT(false, "Unknown aux data policy");
            break;
    }

    txHashBuilder_addAuxData(txHashBuilder,
                             G_context.tx_info.transaction.auxDataHash,
                             AUX_DATA_HASH_LENGTH);
    return SWO_SUCCESS;
}

static int validate_and_hash_validity_interval_start(tx_hash_builder_t* txHashBuilder, tx_ui_plan_t* plan) {
    if (!G_context.tx_info.transaction.includeValidityIntervalStart) {
        return SWO_SUCCESS;
    }

    security_policy_t validity_interval_start_policy = policyForSignTxValidityIntervalStart();
    switch (validity_interval_start_policy) {
        case POLICY_DENY:
            return SWO_SECURITY_CONDITION_NOT_SATISFIED;
        case POLICY_SHOW:
            plan->pair_count += UI_PAIRS_VALIDITY_INTERVAL_START;
            break;
        case POLICY_HIDE:
            break;
        default:
            LEDGER_ASSERT(false, "Unknown validity interval start policy");
            break;
    }

    txHashBuilder_addValidityIntervalStart(
        txHashBuilder,
        G_context.tx_info.transaction.validityIntervalStart
    );
    return SWO_SUCCESS;
}

static int validate_and_hash_mint(tx_hash_builder_t* txHashBuilder, tx_ui_plan_t* plan) {
    if (G_context.tx_info.transaction.num_mint_asset_groups == 0) {
        return SWO_SUCCESS;
    }

    security_policy_t mint_policy =
        policyForSignTxMintInit(G_context.tx_info.transaction.txSigningMode);
    switch (mint_policy) {
        case POLICY_DENY:
            return SWO_SECURITY_CONDITION_NOT_SATISFIED;
        case POLICY_SHOW: {
            plan->pair_count += UI_PAIRS_MINT_SUMMARY;
            uint16_t asset_group_count = 0;
            s_flist_node *node = G_context.tx_info.transaction.mint_asset_groups;
            while (node != NULL) {
                mint_asset_group_node_t *asset_group_node =
                    (mint_asset_group_node_t *) node;
                const mint_asset_group_t *asset_group = &asset_group_node->asset_group;
                if (asset_group->tokens != NULL) {
                    uint16_t token_count = 0;
                    s_flist_node *node2 = asset_group->tokens;
                    while (node2 != NULL) {
                        plan->pair_count += UI_PAIRS_TOKEN;
                        token_count++;
                        node2 = node2->next;
                    }
                    LEDGER_ASSERT(token_count == asset_group->numTokens,
                                  "Mint asset group token count mismatch");
                }
                asset_group_count++;
                node = node->next;
            }
            LEDGER_ASSERT(asset_group_count == G_context.tx_info.transaction.num_mint_asset_groups,
                          "Mint asset group count mismatch");
            break;
        }
        case POLICY_HIDE:
            break;
        default:
            LEDGER_ASSERT(false, "Unknown mint policy");
            break;
    }

    txHashBuilder_enterMint(txHashBuilder);
    txHashBuilder_addMint_topLevelData(txHashBuilder,
                                       G_context.tx_info.transaction.num_mint_asset_groups);

    s_flist_node *node = G_context.tx_info.transaction.mint_asset_groups;
    while (node != NULL) {
        mint_asset_group_node_t *asset_group_node = (mint_asset_group_node_t *) node;
        const mint_asset_group_t *asset_group = &asset_group_node->asset_group;

        txHashBuilder_addMint_tokenGroup(txHashBuilder,
                                         asset_group->policyId,
                                         MINTING_POLICY_ID_LENGTH,
                                         asset_group->numTokens);

        s_flist_node *node2 = asset_group->tokens;
        while (node2 != NULL) {
            mint_token_node_t *token_node = (mint_token_node_t *) node2;
            const mint_token_t *token = &token_node->token;
            txHashBuilder_addMint_token(txHashBuilder,
                                    token->assetName,
                                    token->assetNameLen,
                                    (uint64_t) token->amount);
            node2 = node2->next;
        }

        node = node->next;
    }

    return SWO_SUCCESS;
}

static int validate_and_hash_script_data_hash(tx_hash_builder_t* txHashBuilder, tx_ui_plan_t* plan) {
    if (!G_context.tx_info.transaction.includeScriptDataHash) {
        return SWO_SUCCESS;
    }

    security_policy_t policy = policyForSignTxScriptDataHash(
        G_context.tx_info.transaction.txSigningMode
    );

    switch (policy) {
        case POLICY_DENY:
            TRACE("Script data hash security policy denied");
            return SWO_SECURITY_CONDITION_NOT_SATISFIED;
        case POLICY_SHOW:
            plan->pair_count += UI_PAIRS_SCRIPT_DATA_HASH;
            break;
        case POLICY_HIDE:
            break;
        default:
            LEDGER_ASSERT(false, "Unknown script data hash policy");
            break;
    }

    txHashBuilder_addScriptDataHash(txHashBuilder,
                                    G_context.tx_info.transaction.scriptDataHash,
                                    SCRIPT_DATA_HASH_LENGTH);
    return SWO_SUCCESS;
}

static int validate_and_hash_collateral_inputs(tx_hash_builder_t* txHashBuilder, tx_ui_plan_t* plan) {
    if (G_context.tx_info.transaction.num_collateral_inputs == 0) {
        return SWO_SUCCESS;
    }

    txHashBuilder_enterCollateralInputs(txHashBuilder);
    s_flist_node *node = G_context.tx_info.transaction.collateral_inputs;
    while (node != NULL) {
        tx_collateral_input_node_t *input_node = (tx_collateral_input_node_t *) node;
        const tx_input_t *input = &input_node->input;

        security_policy_t collateral_input_policy = policyForSignTxCollateralInput(
            G_context.tx_info.transaction.txSigningMode,
            G_context.tx_info.transaction.includeTotalCollateral,
            input
        );

        switch (collateral_input_policy) {
            case POLICY_DENY:
                TRACE("Collateral input security policy denied");
                return SWO_SECURITY_CONDITION_NOT_SATISFIED;
            case POLICY_SHOW:
                plan->pair_count += UI_PAIRS_COLLATERAL_INPUT;
                break;
            case POLICY_HIDE:
                break;
            default:
                LEDGER_ASSERT(false, "Unknown collateral input policy");
                break;
        }

        txHashBuilder_addCollateralInput(txHashBuilder, input);
        node = node->next;
    }

    return SWO_SUCCESS;
}

static int validate_and_hash_required_signers(tx_hash_builder_t* txHashBuilder, tx_ui_plan_t* plan) {
    if (G_context.tx_info.transaction.num_required_signers == 0) {
        return SWO_SUCCESS;
    }

    txHashBuilder_enterRequiredSigners(txHashBuilder);
    s_flist_node *node = G_context.tx_info.transaction.required_signers;
    while (node != NULL) {
        tx_required_signer_node_t *signer_node = (tx_required_signer_node_t *) node;
        required_signer_t *required_signer = &signer_node->required_signer;

        security_policy_t signer_policy = policyForSignTxRequiredSigner(
            G_context.tx_info.transaction.txSigningMode,
            required_signer
        );

        switch (signer_policy) {
            case POLICY_DENY:
                TRACE("Required signer security policy denied");
                return SWO_SECURITY_CONDITION_NOT_SATISFIED;
            case POLICY_SHOW:
                plan->pair_count += UI_PAIRS_REQUIRED_SIGNER;
                break;
            case POLICY_HIDE:
                break;
            default:
                LEDGER_ASSERT(false, "Unknown required signer policy");
                break;
        }

        uint8_t keyHash[ADDRESS_KEY_HASH_LENGTH];
        if (required_signer->type == REQUIRED_SIGNER_WITH_PATH) {
            bip44_pathToKeyHash(&required_signer->keyPath, keyHash, sizeof(keyHash));
        } else {
            ASSERT(required_signer->type == REQUIRED_SIGNER_WITH_HASH);
            LEDGER_ASSERT(required_signer->keyHash != NULL, "NULL required signer key hash");
            memmove(keyHash, required_signer->keyHash, sizeof(keyHash));
        }
        txHashBuilder_addRequiredSigner(txHashBuilder, keyHash, sizeof(keyHash));

        node = node->next;
    }

    return SWO_SUCCESS;
}

static int validate_and_hash_network_id(tx_hash_builder_t* txHashBuilder) {
    if (!G_context.tx_info.transaction.includeNetworkId) {
        return SWO_SUCCESS;
    }

    txHashBuilder_addNetworkId(txHashBuilder, G_context.tx_info.transaction.networkId);
    return SWO_SUCCESS;
}

static int validate_and_hash_collateral_output(tx_hash_builder_t* txHashBuilder, tx_ui_plan_t* plan) {
    if (!G_context.tx_info.transaction.includeCollateralOutput) {
        return SWO_SUCCESS;
    }

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

    if (collateral_policy == POLICY_SHOW &&
        collateral_desc.numAssetGroups > 0) {
        warning_bits_set(&G_context.tx_info.warning_bits, WARNING_BIT_COLLATERAL_OUTPUT_WARNING);
    }

    switch (collateral_policy) {
        case POLICY_DENY:
            TRACE("Collateral output security policy denied");
            return SWO_SECURITY_CONDITION_NOT_SATISFIED;
        case POLICY_SHOW: {
            plan->pair_count += UI_PAIRS_COLLATERAL_OUTPUT_ADDRESS;
            if (G_context.tx_info.transaction.collateral_output.destination.type == DESTINATION_DEVICE_OWNED) {
                plan->pair_count += UI_PAIRS_COLLATERAL_OUTPUT_DEVICE_OWNED;
            }
            if (collateral_ada_policy == POLICY_SHOW) {
                plan->pair_count += UI_PAIRS_COLLATERAL_OUTPUT_AMOUNT;
            }
            if (collateral_tokens_policy == POLICY_SHOW &&
                G_context.tx_info.transaction.collateral_output.assetGroups != NULL) {
                uint16_t collateral_group_count = 0;
                s_flist_node *node2 =
                    G_context.tx_info.transaction.collateral_output.assetGroups;
                while (node2 != NULL) {
                    output_asset_group_node_t *asset_group_node =
                        (output_asset_group_node_t *) node2;
                    const output_asset_group_t *asset_group = &asset_group_node->asset_group;
                    {
                        uint16_t token_count = 0;
                        s_flist_node *node3 = asset_group->tokens;
                        while (node3 != NULL) {
                            plan->pair_count += UI_PAIRS_TOKEN;
                            token_count++;
                            node3 = node3->next;
                        }
                        LEDGER_ASSERT(token_count == asset_group->numTokens,
                                      "Collateral asset group token count mismatch");
                    }
                    collateral_group_count++;
                    node2 = node2->next;
                }
                LEDGER_ASSERT(collateral_group_count ==
                              G_context.tx_info.transaction.collateral_output.numAssetGroups,
                              "Collateral asset group count mismatch");
            }
            break;
        }
        case POLICY_HIDE:
            break;
        default:
            LEDGER_ASSERT(false, "Unknown collateral output policy");
            break;
    }

    if (collateral_desc.destination.type == DESTINATION_THIRD_PARTY) {
        txHashBuilder_addCollateralOutput(txHashBuilder, &collateral_desc);
    } else {
        uint8_t *address_bytes = (uint8_t *) app_mem_alloc(MAX_ADDRESS_LENGTH);
        if (address_bytes == NULL) {
            return SWO_INSUFFICIENT_MEMORY;
        }

        size_t address_size = deriveAddress(
            &G_context.tx_info.transaction.collateral_output.destination.params,
            address_bytes,
            MAX_ADDRESS_LENGTH
        );

        if (address_size == 0 || address_size > MAX_ADDRESS_LENGTH) {
            app_mem_free(address_bytes);
            return SWO_INCORRECT_DATA;
        }

        collateral_desc.destination.type = DESTINATION_THIRD_PARTY;
        collateral_desc.destination.address.buffer = address_bytes;
        collateral_desc.destination.address.size = address_size;
        txHashBuilder_addCollateralOutput(txHashBuilder, &collateral_desc);
        app_mem_free(address_bytes);
    }

    uint16_t collateral_group_count = 0;
    s_flist_node *node2 = G_context.tx_info.transaction.collateral_output.assetGroups;
    while (node2 != NULL) {
        output_asset_group_node_t *asset_group_node =
            (output_asset_group_node_t *) node2;
        const output_asset_group_t *asset_group = &asset_group_node->asset_group;
        txHashBuilder_addCollateralOutput_tokenGroup(txHashBuilder,
                                                     asset_group->policyId,
                                                     MINTING_POLICY_ID_LENGTH,
                                                     asset_group->numTokens);

        s_flist_node *node3 = asset_group->tokens;
        while (node3 != NULL) {
            output_token_node_t *token_node = (output_token_node_t *) node3;
            const output_token_t *token = &token_node->token_data;
            txHashBuilder_addCollateralOutput_token(txHashBuilder,
                                                    token->assetName,
                                                    token->assetNameLen,
                                                    (uint64_t) token->amount);
            node3 = node3->next;
        }
        collateral_group_count++;
        node2 = node2->next;
    }
    LEDGER_ASSERT(collateral_group_count ==
                  G_context.tx_info.transaction.collateral_output.numAssetGroups,
                  "Collateral asset group count mismatch");

    return SWO_SUCCESS;
}

static int validate_and_hash_total_collateral(tx_hash_builder_t* txHashBuilder, tx_ui_plan_t* plan) {
    if (!G_context.tx_info.transaction.includeTotalCollateral) {
        return SWO_SUCCESS;
    }

    security_policy_t policy = policyForSignTxTotalCollateral();

    switch (policy) {
        case POLICY_DENY:
            TRACE("Total collateral security policy denied");
            return SWO_SECURITY_CONDITION_NOT_SATISFIED;
        case POLICY_SHOW:
            plan->pair_count += UI_PAIRS_TOTAL_COLLATERAL;
            break;
        case POLICY_HIDE:
            break;
        default:
            LEDGER_ASSERT(false, "Unknown total collateral policy");
            break;
    }

    txHashBuilder_addTotalCollateral(txHashBuilder, G_context.tx_info.transaction.totalCollateral);
    return SWO_SUCCESS;
}

static int validate_and_hash_reference_inputs(tx_hash_builder_t* txHashBuilder, tx_ui_plan_t* plan) {
    if (G_context.tx_info.transaction.num_reference_inputs == 0) {
        return SWO_SUCCESS;
    }

    txHashBuilder_enterReferenceInputs(txHashBuilder);
    s_flist_node *node = G_context.tx_info.transaction.reference_inputs;
    while (node != NULL) {
        tx_input_node_t *input_node = (tx_input_node_t *) node;
        const tx_input_t *input = &input_node->input;

        security_policy_t reference_input_policy = policyForSignTxReferenceInput(
            G_context.tx_info.transaction.txSigningMode,
            input
        );

        switch (reference_input_policy) {
            case POLICY_DENY:
                TRACE("Reference input security policy denied");
                return SWO_SECURITY_CONDITION_NOT_SATISFIED;
            case POLICY_SHOW:
                plan->pair_count += UI_PAIRS_REFERENCE_INPUT;
                break;
            case POLICY_HIDE:
                break;
            default:
                LEDGER_ASSERT(false, "Unknown reference input policy");
                break;
        }

        txHashBuilder_addReferenceInput(txHashBuilder, input);
        node = node->next;
    }

    return SWO_SUCCESS;
}

static int validate_and_hash_voting_procedures(tx_hash_builder_t* txHashBuilder, tx_ui_plan_t* plan) {
    if (G_context.tx_info.transaction.num_voters == 0) {
        return SWO_SUCCESS;
    }

    txHashBuilder_enterVotingProcedures(txHashBuilder);

    uint8_t previous_voter_key[MAX_CBOR_VOTER_MAP_KEY_SIZE];
    size_t previous_voter_key_len = 0;
    bool has_previous_voter_key = false;

    s_flist_node *node = G_context.tx_info.transaction.voting_procedures;
    while (node != NULL) {
        voter_votes_node_t *voter_node = (voter_votes_node_t *) node;
        voter_votes_t *voter_votes = &voter_node->voter_votes_data;

        security_policy_t voter_policy = policyForSignTxVotingProcedure(
            G_context.tx_info.transaction.txSigningMode,
            &voter_votes->voter
        );

        switch (voter_policy) {
            case POLICY_DENY:
                return SWO_SECURITY_CONDITION_NOT_SATISFIED;
            case POLICY_SHOW: {
                plan->pair_count += UI_PAIRS_VOTER;
                s_flist_node *node2 = voter_votes->votes;
                while (node2 != NULL) {
                    vote_node_t *vote_node = (vote_node_t *) node2;
                    const vote_item_t *vote_data = &vote_node->vote_data;
                    plan->pair_count += UI_PAIRS_VOTE;
                    if (vote_data->anchor.isIncluded) {
                        plan->pair_count += UI_PAIRS_ANCHOR;
                    }
                    node2 = node2->next;
                }
                break;
            }
            case POLICY_HIDE:
                break;
            default:
                LEDGER_ASSERT(false, "Unknown voter policy");
                break;
        }

        voter_t voter_for_hash = _voterForTxHash(&voter_votes->voter);

        uint8_t voter_key[MAX_CBOR_VOTER_MAP_KEY_SIZE];
        size_t voter_key_len = txHashBuilder_serializeVoterKey(
            &voter_for_hash,
            voter_key,
            sizeof(voter_key));

        if (has_previous_voter_key &&
            !cbor_mapKeyFulfillsCanonicalOrdering(
                previous_voter_key,
                previous_voter_key_len,
                voter_key,
                voter_key_len)) {
            TRACE("Voting procedures not in canonical order");
            return SWO_TX_PARSING_FAIL_VOTING_PROCEDURES;
        }

        memcpy(previous_voter_key, voter_key, voter_key_len);
        previous_voter_key_len = voter_key_len;
        has_previous_voter_key = true;

        txHashBuilder_addVoter(txHashBuilder,
                               &voter_for_hash,
                               voter_votes->numVotes);

        uint8_t previous_vote_key[MAX_CBOR_GOV_ACTION_MAP_KEY_SIZE];
        size_t previous_vote_key_len = 0;
        bool has_previous_vote_key = false;
        {
            s_flist_node *node2 = voter_votes->votes;
            while (node2 != NULL) {
                vote_node_t *vote_node = (vote_node_t *) node2;
                const vote_item_t *vote_data = &vote_node->vote_data;

                uint8_t gov_action_key[MAX_CBOR_GOV_ACTION_MAP_KEY_SIZE];
                size_t gov_action_key_len = txHashBuilder_serializeGovActionKey(
                        &vote_data->govActionId,
                        gov_action_key,
                        sizeof(gov_action_key));

                if (has_previous_vote_key &&
                    !cbor_mapKeyFulfillsCanonicalOrdering(
                        previous_vote_key,
                        previous_vote_key_len,
                        gov_action_key,
                        gov_action_key_len)) {
                    TRACE("Votes not in canonical order");
                    return SWO_TX_PARSING_FAIL_VOTING_PROCEDURES;
                }

                memcpy(previous_vote_key, gov_action_key, gov_action_key_len);
                previous_vote_key_len = gov_action_key_len;
                has_previous_vote_key = true;

                voting_procedure_t voting_procedure = {
                    .vote = vote_data->voteOption,
                    .anchor = vote_data->anchor
                };

                gov_action_id_t gov_action_id = vote_data->govActionId;
                txHashBuilder_addVote(txHashBuilder,
                                     &gov_action_id,
                                     &voting_procedure);

                node2 = node2->next;
            }
        }

        node = node->next;
    }

    return SWO_SUCCESS;
}

static int validate_and_hash_treasury(tx_hash_builder_t* txHashBuilder, tx_ui_plan_t* plan) {
    if (!G_context.tx_info.transaction.includeTreasury) {
        return SWO_SUCCESS;
    }

    security_policy_t treasury_policy = policyForSignTxTreasury(
        G_context.tx_info.transaction.txSigningMode,
        G_context.tx_info.transaction.treasury
    );
    switch (treasury_policy) {
        case POLICY_DENY:
            return SWO_SECURITY_CONDITION_NOT_SATISFIED;
        case POLICY_SHOW:
            plan->pair_count += UI_PAIRS_TREASURY;
            break;
        case POLICY_HIDE:
            break;
        default:
            LEDGER_ASSERT(false, "Unknown treasury policy");
            break;
    }

    txHashBuilder_addTreasury(txHashBuilder, G_context.tx_info.transaction.treasury);
    return SWO_SUCCESS;
}

static int validate_and_hash_donation(tx_hash_builder_t* txHashBuilder, tx_ui_plan_t* plan) {
    if (!G_context.tx_info.transaction.includeDonation) {
        return SWO_SUCCESS;
    }

    security_policy_t donation_policy = policyForSignTxDonation(
        G_context.tx_info.transaction.txSigningMode,
        G_context.tx_info.transaction.donation
    );
    switch (donation_policy) {
        case POLICY_DENY:
            return SWO_SECURITY_CONDITION_NOT_SATISFIED;
        case POLICY_SHOW:
            plan->pair_count += UI_PAIRS_DONATION;
            break;
        case POLICY_HIDE:
            break;
        default:
            LEDGER_ASSERT(false, "Unknown donation policy");
            break;
    }

    txHashBuilder_addDonation(txHashBuilder, G_context.tx_info.transaction.donation);
    return SWO_SUCCESS;
}

// NOTE: Validation consists of
// (1) calling security policies,
// (2) checking canonical ordering of CBOR map keys.
// It is not possible to check (2) without calling tx hash builder,
// and the tx hash builder has a very tight state machine
// and cannot be used only partially.
// UI planning is based on security policies, so if we do not want to call them
// twice, it also does not make sense to separate it.
int tx_validate_and_compute_hash(tx_ui_plan_t* plan) {
    LEDGER_ASSERT(G_context.state.tx_state == TX_STATE_PARSED, "Validation invoked at wrong state");
    LEDGER_ASSERT(plan != NULL, "NULL plan");

    TRACE("Expert mode: %d", is_expert_mode());

    G_context.tx_info.pool_owner_path_present = false;
    plan->pair_count = 0;
    plan->has_excessive_length_element = false;  // TODO: Implement detection during validation

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

    int status = validate_and_hash_inputs(&txHashBuilder, plan);
    if (status != SWO_SUCCESS) return status;
    status = validate_and_hash_outputs(&txHashBuilder, plan);
    if (status != SWO_SUCCESS) return status;
    status = validate_and_hash_fee(&txHashBuilder, plan);
    if (status != SWO_SUCCESS) return status;
    status = validate_and_hash_ttl(&txHashBuilder, plan);
    if (status != SWO_SUCCESS) return status;
    status = validate_and_hash_certificates(&txHashBuilder, plan);
    if (status != SWO_SUCCESS) return status;
    status = validate_and_hash_withdrawals(&txHashBuilder, plan);
    if (status != SWO_SUCCESS) return status;
    status = validate_and_hash_aux_data_hash(&txHashBuilder, plan);
    if (status != SWO_SUCCESS) return status;
    status = validate_and_hash_validity_interval_start(&txHashBuilder, plan);
    if (status != SWO_SUCCESS) return status;
    status = validate_and_hash_mint(&txHashBuilder, plan);
    if (status != SWO_SUCCESS) return status;
    status = validate_and_hash_script_data_hash(&txHashBuilder, plan);
    if (status != SWO_SUCCESS) return status;
    status = validate_and_hash_collateral_inputs(&txHashBuilder, plan);
    if (status != SWO_SUCCESS) return status;
    status = validate_and_hash_required_signers(&txHashBuilder, plan);
    if (status != SWO_SUCCESS) return status;
    status = validate_and_hash_network_id(&txHashBuilder);
    if (status != SWO_SUCCESS) return status;
    status = validate_and_hash_collateral_output(&txHashBuilder, plan);
    if (status != SWO_SUCCESS) return status;
    status = validate_and_hash_total_collateral(&txHashBuilder, plan);
    if (status != SWO_SUCCESS) return status;
    status = validate_and_hash_reference_inputs(&txHashBuilder, plan);
    if (status != SWO_SUCCESS) return status;
    status = validate_and_hash_voting_procedures(&txHashBuilder, plan);
    if (status != SWO_SUCCESS) return status;
    status = validate_and_hash_treasury(&txHashBuilder, plan);
    if (status != SWO_SUCCESS) return status;
    status = validate_and_hash_donation(&txHashBuilder, plan);
    if (status != SWO_SUCCESS) return status;

    txHashBuilder_finalize(&txHashBuilder,
                          G_context.tx_info.tx_hash,
                          sizeof(G_context.tx_info.tx_hash));

    TRACE("Hash: %.*H", sizeof(G_context.tx_info.tx_hash), G_context.tx_info.tx_hash);

    security_policy_t tx_hash_policy =
        policyForSignTxDisplayTxHash(G_context.tx_info.transaction.txSigningMode);
    switch (tx_hash_policy) {
        case POLICY_SHOW:
            plan->pair_count += UI_PAIRS_TX_HASH;
            break;
        case POLICY_HIDE:
            break;
        default:
            LEDGER_ASSERT(false, "Unknown tx hash display policy");
    }

    // TODO: implement streaming review flow and remove this assertion once we're handling overflow.
    LEDGER_ASSERT(plan->pair_count <= UI_PAIR_LIMIT, "Need streaming UI fallback");

    return SWO_SUCCESS;
}
