#include <stddef.h>  // NULL
#include <stdint.h>
#include <stdbool.h>

#include "tx_prepare.h"

#include "os.h"
#include "app_tokens/app_tokens.h"
#include "cardano_swo.h"
#include "globals.h"
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

int compute_tx_hash_and_plan_ui(tx_ui_plan_t* plan) {
    LEDGER_ASSERT(G_context.state.tx_state == TX_STATE_PARSED, "Hash planning invoked at wrong state");
    LEDGER_ASSERT(plan != NULL, "NULL plan");

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
                      0,
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
            uint8_t *address_bytes = (uint8_t *) app_mem_alloc(MAX_ADDRESS_SIZE);
            if (address_bytes == NULL) {
                return send_error_and_reset(SWO_TX_PARSING_FAIL);
            }

            size_t address_size = deriveAddress(
                &output_item->output_data.destination.params,
                address_bytes,
                MAX_ADDRESS_SIZE
            );

            if (address_size == 0 || address_size > MAX_ADDRESS_SIZE) {
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

            uint8_t reward_address[REWARD_ACCOUNT_SIZE];
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

            if (reward_addr_len == 0 || reward_addr_len != REWARD_ACCOUNT_SIZE) {
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

    txHashBuilder_finalize(&txHashBuilder,
                          G_context.tx_info.tx_hash,
                          sizeof(G_context.tx_info.tx_hash));

    TRACE("Hash: %.*H", sizeof(G_context.tx_info.tx_hash), G_context.tx_info.tx_hash);

    // TODO: implement streaming review flow and remove this assertion once we're handling overflow.
    LEDGER_ASSERT(plan->pair_count <= UI_PAIR_LIMIT, "Need streaming UI fallback");
    LEDGER_ASSERT(plan->pair_count <= UINT8_MAX, "Pair count overflow");

    return SWO_SUCCESS;
}
