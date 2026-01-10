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

/**
 * @file tx_ui_format.c
 * @brief Transaction UI formatting (Phase 2 of 2-phase architecture)
 *
 * This file implements Phase 2 of transaction processing: formatting the validated
 * transaction into human-readable strings for display on the device.
 *
 * ## Architecture Overview
 *
 * **Phase 1** (tx_validate.c) has already:
 * - Validated the transaction structure
 * - Run security policies (rejecting unsafe transactions)
 * - Computed the transaction hash
 * - Counted how many UI pairs will be needed
 *
 * **Phase 2** (this file) now:
 * - Formats each transaction element into display strings
 * - Builds NBGL key-value pairs for the UI
 * - Constructs warning structures
 * - Frees parsed transaction data after formatting
 *
 * ## CRITICAL SYNCHRONIZATION REQUIREMENT
 *
 * The number of UI pairs added in this file MUST EXACTLY MATCH the count from Phase 1.
 * This is verified by a runtime ASSERT (line ~2040).
 *
 * **When adding new displayable fields:**
 * 1. Update tx_validate.c to count the additional pairs
 * 2. Update this file to format and display those pairs
 * 3. Ensure both files iterate elements in IDENTICAL order
 *
 * **Example:** Device-owned addresses show 2 extra pairs:
 * - tx_validate.c: `plan->pair_count += 2`  (line 257, 1269)
 * - tx_ui_format.c: calls `addPaymentInfoUIPair()` + `addStakingInfoUIPair()` (line 263, 1655)
 *
 * See tx_validate.h for complete architecture documentation.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "os.h"
#include "glyphs.h"
#include "nbgl_use_case.h"
#include "cardano_swo.h"
#include "globals.h"
#include "format.h"
#include "ui/ui_constants.h"
#include "tx_output_types.h"
#include "transaction/tx.h"
#include "addressUtils/addressUtilsShelley.h"
#include "memory/mem.h"
#include "securityPolicy/securityPolicy.h"
#include "securityPolicy/securityWarnings.h"
#include "transaction/tx_utils.h"
#include "utils/assert.h"
#include "utils/textUtils.h"
#include "utils/ipUtils.h"
#include "ui/ui_utils.h"
#include "ui/ui_formatters.h"
#include "ui/ui_warnings.h"
#include "ui/ui_display_tx.h"
#include "ui/tx_ui_helpers.h"
#include "io.h"
#include "app_context.h"
#include "app_tokens/app_tokens.h"
#include "transaction/tx_voting_procedure_types.h"
#include "addressUtils/bech32.h"

// Max display lengths
#define MAX_DATUM_HASH_STRING_LENGTH (2 * OUTPUT_DATUM_HASH_LENGTH + 1)
#define MAX_POOL_METADATA_HASH_STRING_LENGTH (2 * POOL_METADATA_HASH_LENGTH + 1)
#define MAX_INPUT_DISPLAY_STRING_LENGTH (MAX_TX_HASH_DISPLAY_LENGTH + 3 + MAX_UINT64_STRING_LENGTH)

static bool format_input_with_index(const tx_input_t *input, char *out, size_t out_size) {
    LEDGER_ASSERT(out != NULL, "NULL output buffer");
    LEDGER_ASSERT(input != NULL, "NULL input");
    int hex_status = bytes_to_lowercase_hex(out, out_size, input->txHash, TX_HASH_LENGTH);
    if (hex_status != 0) {
        return false;
    }
    size_t hash_len = strlen(out);
    if (hash_len + 1 >= out_size) {
        return false;
    }
    snprintf(out + hash_len, out_size - hash_len, " / %u", input->index);
    LEDGER_ASSERT(strlen(out) + 1 <= out_size, "Input display buffer overflow");
    return true;
}

static void ui_format_token_groups(asset_group_t* assetGroups,
                                        uint16_t numGroups,
                                        bool show_tokens) {
    if (assetGroups == NULL) {
        return;
    }

    for (uint16_t ag = 0; ag < numGroups; ag++) {
        asset_group_t *group = &assetGroups[ag];
        s_flist_node *token_node = group->tokens;
        while (token_node != NULL) {
            output_token_list_item_t *token_item = (output_token_list_item_t *) token_node;
            output_token_t *token = &token_item->token_data;
            s_flist_node *token_next = token_node->next;

            if (show_tokens) {
                token_group_t tokenGroup;
                memcpy(tokenGroup.policyId, group->policyId, MINTING_POLICY_ID_LENGTH);

                UI_ADD_FORMAT3(UI_STATIC_LABEL("Asset fingerprint"), MAX_TOKEN_FINGERPRINT_STRING_LENGTH, format_asset_fingerprint_bech32, &tokenGroup, token->assetName, token->assetNameLen);
                UI_ADD_FORMAT4(UI_STATIC_LABEL("Token amount"), MAX_TOKEN_AMOUNT_OUTPUT_STRING_LENGTH, format_token_amount_output, &tokenGroup, token->assetName, token->assetNameLen, token->amount);
            }

            app_mem_free(token_node);
            token_node = token_next;
        }
        group->tokens = NULL;
    }
}

// Helper function to format output address (handles both third-party and device-owned destinations)
static bool format_output_address(const tx_output_description_t *output_desc, char *out, size_t out_size) {
    LEDGER_ASSERT(output_desc != NULL, "NULL output_desc");
    LEDGER_ASSERT(out != NULL, "NULL output buffer");

    if (output_desc->destination.type == DESTINATION_THIRD_PARTY) {
        return format_address_human_readable(
            output_desc->destination.address.buffer,
            output_desc->destination.address.size,
            out,
            out_size);
    } else {
        uint8_t address_bytes[MAX_ADDRESS_LENGTH] = {0};
        size_t derived_len = deriveAddress(
            output_desc->destination.params,
            address_bytes,
            sizeof(address_bytes));
        if (derived_len > 0) {
            return format_address_human_readable(
                address_bytes,
                derived_len,
                out,
                out_size);
        }
        return false;
    }
}

static void ui_strings_inputs(transaction_t *tx) {
    security_policy_t input_policy = policyForSignTxInput(tx->txSigningMode);
    LEDGER_ASSERT(input_policy != POLICY_DENY, "Input denied during UI");
    s_flist_node *input_node = tx->inputs;
    while (input_node != NULL) {
        tx_input_list_item_t *input_item = (tx_input_list_item_t *) input_node;
        s_flist_node *next = input_node->next;

        if (input_policy == POLICY_SHOW) {
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Input"), MAX_INPUT_DISPLAY_STRING_LENGTH, format_input_with_index, &input_item->input_data);
        }

        app_mem_free(input_item);
        input_node = next;
    }
    tx->inputs = NULL;
}

static void ui_strings_outputs(transaction_t *tx) {
    uint16_t output_num = 1;
    s_flist_node *output_node = tx->outputs;
    TRACE("Materializing %u outputs", tx->num_outputs);
    while (output_node != NULL) {
        tx_output_list_item_t *output_item = (tx_output_list_item_t *) output_node;
        s_flist_node *next_node = output_node->next;

        tx_output_description_t output_desc = {
            .format = output_item->output_data.format,
            .amount = output_item->output_data.adaAmount,
            .numAssetGroups = output_item->output_data.numAssetGroups,
            .includeDatum = output_item->output_data.datum.hasDatum,
            .includeRefScript = output_item->output_data.hasRefScript,
        };

        if (output_item->output_data.destination.type == DESTINATION_THIRD_PARTY) {
            output_desc.destination.type = DESTINATION_THIRD_PARTY;
            output_desc.destination.address.buffer = output_item->output_data.destination.address.buffer;
            output_desc.destination.address.size = output_item->output_data.destination.address.size;
        } else {
            output_desc.destination.type = DESTINATION_DEVICE_OWNED;
            output_desc.destination.params = &output_item->output_data.destination.params;
        }

        security_policy_t policy = (output_desc.destination.type == DESTINATION_THIRD_PARTY)
            ? policyForSignTxOutputAddressBytes(
                &output_desc,
                tx->txSigningMode,
                tx->networkId,
                tx->protocolMagic,
                &G_context.tx_info.warning_bits)
            : policyForSignTxOutputAddressParams(
                &output_desc,
                tx->txSigningMode,
                tx->networkId,
                tx->protocolMagic,
                &G_context.tx_info.warning_bits);

        LEDGER_ASSERT(policy != POLICY_DENY, "Output denied during UI");

        security_policy_t datum_policy = policyForSignTxOutputDatumHash(policy);
        LEDGER_ASSERT(datum_policy != POLICY_DENY, "Output datum policy denied during UI");
        security_policy_t ref_script_policy = policyForSignTxOutputRefScript(policy);
        LEDGER_ASSERT(ref_script_policy != POLICY_DENY, "Output ref script policy denied during UI");

        switch (policy) {
            case POLICY_DENY:
                // Already asserted above, this case should never be reached
                break;
            case POLICY_SHOW: {
                TRACE("Materializing output #%u", output_num);
                UI_ADD_FORMAT1(UI_STATIC_LABEL("Output"), MAX_UINT64_STRING_LENGTH, format_index_with_prefix, output_num);
                UI_ADD_FORMAT1(UI_STATIC_LABEL("Address"), MAX_HUMAN_ADDRESS_LENGTH, format_output_address, &output_desc);

                // For device-owned addresses, show payment and staking details
                if (output_item->output_data.destination.type == DESTINATION_DEVICE_OWNED) {
                    addPaymentInfoUIPair(&output_item->output_data.destination.params);
                    addStakingInfoUIPair(&output_item->output_data.destination.params);
                }

                UI_ADD_FORMAT1(UI_STATIC_LABEL("Amount"), MAX_ADA_AMOUNT_STRING_LENGTH, str_formatAdaAmount, output_item->output_data.adaAmount);
                if (output_item->output_data.datum.hasDatum && datum_policy == POLICY_SHOW) {
                    // TODO: Inline datum size is not bounded by protocol; handle large values more robustly.
                    if (output_item->output_data.datum.type == DATUM_HASH) {
                        UI_ADD_FORMAT2(UI_STATIC_LABEL("Datum hash"), MAX_DATUM_HASH_STRING_LENGTH, format_hex_ui, output_item->output_data.datum.hash, OUTPUT_DATUM_HASH_LENGTH);
                    } else {
                        // TODO not enough space for inline datum, how big a buffer to use here?
                        // TODO these unlimited items in UI should perhaps be detected upfront, we can go over the whole tx and check if some individual field
                        // TODO is too big for UI and run it via some streaming UI
                        const int max_len = 100;
                        char *datum_value_tmp = (char *) app_mem_alloc(max_len + 2);
                        if (datum_value_tmp == NULL) {
                            ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
                        } else {
                            uint16_t inline_size = output_item->output_data.datum.inline_data.size;
                            snprintf(datum_value_tmp, max_len + 2, "Inline datum (%u bytes)", inline_size);
                            LEDGER_ASSERT(strlen(datum_value_tmp) <= MAX_DATUM_HASH_STRING_LENGTH, "Datum ui string buffer too short");

                            if (!ui_pairs_add_static_label(UI_STATIC_LABEL("Inline datum"), datum_value_tmp)) {
                                ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
                            }
                        }
                    }
                }

                if (output_item->output_data.hasRefScript && ref_script_policy == POLICY_SHOW) {
                    // TODO: Reference script size is not bounded by protocol; handle large values more robustly.
                    char *refscript_tmp = (char *) app_mem_alloc(MAX_REFERENCE_SCRIPT_STRING_LENGTH + UI_BUFFER_SAFETY_MARGIN);
                    if (refscript_tmp == NULL) {
                        ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
                    } else {
                        snprintf(refscript_tmp,
                                 MAX_REFERENCE_SCRIPT_STRING_LENGTH + UI_BUFFER_SAFETY_MARGIN,
                                 "Reference script (%u bytes)",
                                 output_item->output_data.refScript.size);
                        LEDGER_ASSERT(strlen(refscript_tmp) <= MAX_REFERENCE_SCRIPT_STRING_LENGTH, "Reference script ui string buffer too short");
                        if (!ui_pairs_add_static_label(UI_STATIC_LABEL("Reference script"), refscript_tmp)) {
                            ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
                        }
                    }
                }

                if (output_item->output_data.assetGroups != NULL) {
                    ui_format_token_groups(
                        output_item->output_data.assetGroups,
                        output_item->output_data.numAssetGroups,
                        true
                    );
                }

                output_num++;
                break;
            }
            case POLICY_HIDE:
                break;
        }

        if (output_item->output_data.assetGroups != NULL) {
            for (uint16_t ag = 0; ag < output_item->output_data.numAssetGroups; ag++) {
                s_flist_node *token_node = output_item->output_data.assetGroups[ag].tokens;
                while (token_node != NULL) {
                    s_flist_node *token_next = token_node->next;
                    app_mem_free(token_node);
                    token_node = token_next;
                }
                output_item->output_data.assetGroups[ag].tokens = NULL;
            }
            app_mem_free(output_item->output_data.assetGroups);
            output_item->output_data.assetGroups = NULL;
        }
        // Note: inline datum and reference script data are pointers into the raw_tx buffer,
        // not separately allocated, so they do not need to be freed
        app_mem_free(output_item);
        output_node = next_node;
    }
    tx->outputs = NULL;
}

static void ui_strings_fee(transaction_t *tx) {
    UI_ADD_FORMAT1(UI_STATIC_LABEL("Fee"), MAX_ADA_AMOUNT_STRING_LENGTH, str_formatAdaAmount, tx->fee);
}

static void ui_strings_ttl(transaction_t *tx) {
    if (!tx->includeTtl) {
        return;
    }
    security_policy_t ttl_policy = policyForSignTxTtl(tx->ttl);
    LEDGER_ASSERT(ttl_policy != POLICY_DENY, "TTL denied during UI");
    switch (ttl_policy) {
        case POLICY_DENY:
            // Already asserted above, this case should never be reached
            break;
        case POLICY_SHOW:
            UI_ADD_FORMAT3(UI_STATIC_LABEL("TTL"), MAX_VALIDITY_BOUNDARY_STRING_LENGTH, str_formatValidityBoundary, tx->ttl, tx->networkId, tx->protocolMagic);
            break;
        case POLICY_HIDE:
            break;
    }
}

// Helper functions for common credential display patterns

static void display_stake_credential(const ext_credential_t* credential) {
    addCredentialUIPairs(
        credential,
        UI_STATIC_LABEL("Stake key"),
        UI_STATIC_LABEL("Stake key hash"),
        "stake_vkh",
        UI_STATIC_LABEL("Stake script hash"),
        "script"
    );
}

static void display_drep_credential(const ext_credential_t* credential) {
    addCredentialUIPairs(
        credential,
        UI_STATIC_LABEL("DRep key"),
        UI_STATIC_LABEL("DRep key hash"),
        "drep",
        UI_STATIC_LABEL("DRep script hash"),
        "drep"
    );
}

static void display_committee_cold_credential(const ext_credential_t* credential) {
    addCredentialUIPairs(
        credential,
        UI_STATIC_LABEL("Committee cold key"),
        UI_STATIC_LABEL("Committee cold key hash"),
        "cc_cold",
        UI_STATIC_LABEL("Committee cold script hash"),
        "cc_cold"
    );
}

static void display_committee_hot_credential(const ext_credential_t* credential) {
    addCredentialUIPairs(
        credential,
        UI_STATIC_LABEL("Committee hot key"),
        UI_STATIC_LABEL("Committee hot key hash"),
        "cc_hot",
        UI_STATIC_LABEL("Committee hot script hash"),
        "cc_hot_script"
    );
}

static void display_voter_credential(const ext_credential_t* credential) {
    addCredentialUIPairs(
        credential,
        UI_STATIC_LABEL("Voter"),
        UI_STATIC_LABEL("Voter hash"),
        "stake_vkh",
        UI_STATIC_LABEL("Voter script hash"),
        "script"
    );
}

static void ui_strings_certificate_stake_registration(const certificate_data_t* certificate_data) {
    display_stake_credential(&certificate_data->stakeCredential);
}

static void ui_strings_certificate_stake_delegation(const certificate_data_t* certificate_data) {
    display_stake_credential(&certificate_data->stakeCredential);
    addPoolKeyHashUIPairs(certificate_data->poolKeyHash, UI_STATIC_LABEL("Pool"));
}

static void ui_strings_certificate_stake_conway(const certificate_data_t* certificate_data) {
    // Display stake credential
    display_stake_credential(&certificate_data->stakeCredential);

    // Display deposit
    addDepositUIPairs(certificate_data->deposit, UI_STATIC_LABEL("Deposit"));
}

static void ui_strings_certificate_pool_retirement(const certificate_data_t* certificate_data) {
    // Display pool credential
    const ext_credential_t* poolCred = &certificate_data->poolCredential;
    uint8_t poolKeyHash[POOL_KEY_HASH_LENGTH];

    switch (poolCred->type) {
        case EXT_CREDENTIAL_KEY_PATH:
            bip44_pathToKeyHash(&poolCred->keyPath, poolKeyHash, sizeof(poolKeyHash));
            break;
        case EXT_CREDENTIAL_KEY_HASH: {
            STATIC_ASSERT(ADDRESS_KEY_HASH_LENGTH == POOL_KEY_HASH_LENGTH,
                            "pool credential hash size mismatch");
            memcpy(poolKeyHash, poolCred->keyHash, POOL_KEY_HASH_LENGTH);
            break;
        }
        default:
            LEDGER_ASSERT(false, "Unsupported pool credential type for retirement");
    }

    // Display pool key hash with "pool" prefix
    addPoolKeyHashUIPairs(poolKeyHash, UI_STATIC_LABEL("Pool ID"));

    // Display retirement epoch
    UI_ADD_FORMAT1(UI_STATIC_LABEL("Retirement epoch"), MAX_UINT64_STRING_LENGTH, format_u64_ui, certificate_data->retirementEpoch);
}

static void ui_strings_certificate_vote_delegation(const certificate_data_t* certificate_data) {
    display_voter_credential(&certificate_data->stakeCredential);

    const ext_drep_t* drep = &certificate_data->drep;
    addDRepUIPairs(drep, UI_STATIC_LABEL("DRep"));
}

static void ui_strings_certificate_committee_hot(const certificate_data_t* certificate_data) {
    const ext_credential_t* coldCred = &certificate_data->coldCredential;
    display_committee_cold_credential(coldCred);

    const ext_credential_t* hotCred = &certificate_data->hotCredential;
    display_committee_hot_credential(hotCred);
}

static void ui_strings_certificate_committee_resign(const certificate_data_t* certificate_data) {
    display_committee_cold_credential(&certificate_data->coldCredential);

    addAnchorUIPairs(&certificate_data->anchor);
}

static void ui_strings_certificate_drep_registration(const certificate_data_t* certificate_data) {
    display_drep_credential(&certificate_data->dRepCredential);

    addDepositUIPairs(certificate_data->deposit, UI_STATIC_LABEL("Deposit"));
    addAnchorUIPairs(&certificate_data->anchor);
}

static void ui_strings_certificate_drep_deregistration(const certificate_data_t* certificate_data) {
    display_drep_credential(&certificate_data->dRepCredential);

    addDepositUIPairs(certificate_data->deposit, UI_STATIC_LABEL("Deposit"));
}

static void ui_strings_certificate_drep_update(const certificate_data_t* certificate_data) {
    display_drep_credential(&certificate_data->dRepCredential);

    addAnchorUIPairs(&certificate_data->anchor);
}

static void ui_strings_certificate_pool_registration(const certificate_data_t* certificate_data, sign_tx_signingmode_t txSigningMode) {
    pool_owner_counts_t pool_owner_counts = count_pool_owner_nodes(
        certificate_data->poolRegistration.poolOwners
    );

    // Check pool ID security policy
    security_policy_t pool_id_policy = policyForSignTxStakePoolRegistrationPoolId(
        txSigningMode,
        &certificate_data->poolId
    );
    LEDGER_ASSERT(pool_id_policy != POLICY_DENY, "Pool ID security policy denied");

    // Display pool ID
    const pool_id_t *poolId = &certificate_data->poolId;
    uint8_t poolKeyHash[POOL_KEY_HASH_LENGTH];

    switch (poolId->keyReferenceType) {
        case KEY_REFERENCE_PATH:
            bip44_pathToKeyHash(&poolId->path, poolKeyHash, sizeof(poolKeyHash));
            break;
        case KEY_REFERENCE_HASH:
            memcpy(poolKeyHash, poolId->hash, POOL_KEY_HASH_LENGTH);
            break;
        default:
            LEDGER_ASSERT(false, "Unsupported pool ID type");
    }

    if (pool_id_policy == POLICY_SHOW) {
        addPoolKeyHashUIPairs(poolKeyHash, UI_STATIC_LABEL("Pool ID"));
    }

    // Check VRF key security policy
    security_policy_t vrf_policy = policyForSignTxStakePoolRegistrationVrfKey(txSigningMode);
    LEDGER_ASSERT(vrf_policy != POLICY_DENY, "VRF key security policy denied");

    // Display VRF key hash
    if (vrf_policy == POLICY_SHOW) {
        UI_ADD_FORMAT3(UI_STATIC_LABEL("VRF key hash"), MAX_BECH32_STRING_LENGTH, format_bech32, "vrf_vk", certificate_data->vrfKeyHash, VRF_KEY_HASH_LENGTH);
    }

    // Display pledge
    UI_ADD_FORMAT1(UI_STATIC_LABEL("Pledge"), MAX_ADA_AMOUNT_STRING_LENGTH, str_formatAdaAmount, certificate_data->poolRegistration.pledge);

    // Display cost
    UI_ADD_FORMAT1(UI_STATIC_LABEL("Cost"), MAX_ADA_AMOUNT_STRING_LENGTH, str_formatAdaAmount, certificate_data->poolRegistration.cost);

    // Display profit margin as percentage
    UI_ADD_FORMAT2(UI_STATIC_LABEL("Profit margin"), MAX_PROFIT_MARGIN_STRING_LENGTH, format_pool_margin,
                   certificate_data->poolRegistration.marginNumerator,
                   certificate_data->poolRegistration.marginDenominator);

    // Check reward account security policy
    security_policy_t reward_policy = policyForSignTxStakePoolRegistrationRewardAccount(
        txSigningMode,
        G_context.tx_info.transaction.networkId,
        &certificate_data->poolRegistration.rewardAccount
    );
    LEDGER_ASSERT(reward_policy != POLICY_DENY, "Reward account security policy denied");

    // Display reward account
    if (reward_policy == POLICY_SHOW) {
        addRewardAccountUIPairs(
            G_context.tx_info.transaction.networkId,
            &certificate_data->poolRegistration.rewardAccount,
            UI_STATIC_LABEL("Pool reward address")
        );
    }

    // Display pool owners
    uint32_t owner_idx = 0;
    s_flist_node* owner_node = certificate_data->poolRegistration.poolOwners;
    while (owner_node != NULL) {
        tx_certificate_list_item_t* owner_item =
            (tx_certificate_list_item_t*) owner_node;
        ext_credential_t* owner_cred = &owner_item->certificate_data.stakeCredential;

        // Check owner security policy
        security_policy_t owner_policy = policyForSignTxStakePoolRegistrationOwner(
            G_context.tx_info.transaction.txSigningMode,
            owner_cred
        );
        LEDGER_ASSERT(owner_policy != POLICY_DENY, "Pool owner security policy denied");

        if (owner_policy == POLICY_SHOW) {
            addRewardAddressFromCredentialUIPairs(
                G_context.tx_info.transaction.networkId,
                owner_cred,
                UI_STATIC_LABEL("Owner reward address")
            );
        }

        owner_node = owner_node->next;
        owner_idx++;
    }

    ASSERT(owner_idx == pool_owner_counts.total_owners);
    if (pool_owner_counts.total_owners == 0) {
        warning_bits_set(&G_context.tx_info.warning_bits,
                            WARNING_BIT_POOL_REGISTRATION_NO_OWNERS);
        if (!ui_pairs_add_static_label_impl(UI_STATIC_LABEL("Pool owners"), (char *) UI_STATIC_LABEL("None"), false)) {
            ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
        }
    }

    // Display relays
    uint32_t relay_idx = 0;
    s_flist_node* relay_node = certificate_data->poolRegistration.relays;
    while (relay_node != NULL) {
        tx_certificate_list_item_t* relay_item =
            (tx_certificate_list_item_t*) relay_node;
        pool_relay_t* relay = (pool_relay_t*) &relay_item->certificate_data;

        // Check relay security policy
        security_policy_t relay_policy = policyForSignTxStakePoolRegistrationRelay(
            G_context.tx_info.transaction.txSigningMode,
            relay
        );
        switch (relay_policy) {
            case POLICY_DENY:
                LEDGER_ASSERT(false, "Relay security policy denied");
                break;
            case POLICY_HIDE:
                break;
            case POLICY_SHOW: {
                // Display relay index
                UI_ADD_FORMAT1(UI_STATIC_LABEL("Relay"), MAX_RELAY_INDEX_STRING_LENGTH, format_index_with_prefix, relay_idx + 1);
                // Display relay format and details
                switch (relay->format) {
                    case RELAY_SINGLE_HOST_IP: {
                        // Display IPv4 if present
                        if (!relay->ipv4.isNull) {
                            UI_ADD_FORMAT1(UI_STATIC_LABEL("IPv4"), MAX_IPV4_STR_LENGTH, format_ipv4, relay->ipv4.ip);
                        }

                        // Display IPv6 if present
                        if (!relay->ipv6.isNull) {
                            UI_ADD_FORMAT1(UI_STATIC_LABEL("IPv6"), MAX_IPV6_STR_LENGTH, format_ipv6, relay->ipv6.ip);
                        }

                        // Display port if present
                        if (!relay->port.isNull) {
                            UI_ADD_FORMAT1(UI_STATIC_LABEL("Port"), MAX_UINT64_STRING_LENGTH, format_u16, relay->port.number);
                        }
                        break;
                    }
                    case RELAY_SINGLE_HOST_NAME: {
                        // Display DNS name
                        if (relay->dnsNameSize > 0) {
                            char *dns_str = (char *) app_mem_alloc(relay->dnsNameSize + 2);
                            if (dns_str == NULL) {
                                ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
                            } else {
                                memcpy(dns_str, relay->dnsName, relay->dnsNameSize);
                                dns_str[relay->dnsNameSize] = '\0';
                                LEDGER_ASSERT(strlen(dns_str) == relay->dnsNameSize, "DNS name length mismatch");
                                if (!ui_pairs_add_static_label(UI_STATIC_LABEL("DNS name"), dns_str)) {
                                    ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
                                }
                            }
                        }

                        // Display port if present
                        if (!relay->port.isNull) {
                            UI_ADD_FORMAT1(UI_STATIC_LABEL("Port"), MAX_UINT64_STRING_LENGTH, format_u16, relay->port.number);
                        }
                        break;
                    }
                    case RELAY_MULTIPLE_HOST_NAME: {
                        // Display DNS name (SRV record)
                        if (relay->dnsNameSize > 0) {
                            char *dns_str = (char *) app_mem_alloc(relay->dnsNameSize + 2);
                            if (dns_str == NULL) {
                                ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
                            } else {
                                memcpy(dns_str, relay->dnsName, relay->dnsNameSize);
                                dns_str[relay->dnsNameSize] = '\0';
                                LEDGER_ASSERT(strlen(dns_str) == relay->dnsNameSize, "SRV DNS name length mismatch");
                                if (!ui_pairs_add_static_label(UI_STATIC_LABEL("SRV DNS"), dns_str)) {
                                    ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
                                }
                            }
                        }
                        break;
                    }
                    default:
                        LEDGER_ASSERT(false, "Unknown relay type");
                }
                break;
            }
        }

        relay_node = relay_node->next;
        relay_idx++;
    }

    ASSERT(relay_idx == certificate_data->poolRegistration.numRelays);
    if (relay_idx == 0) {
        warning_bits_set(&G_context.tx_info.warning_bits,
                            WARNING_BIT_POOL_REGISTRATION_NO_RELAYS);
        if (!ui_pairs_add_static_label_impl(UI_STATIC_LABEL("Pool relays"), (char *) UI_STATIC_LABEL("None"), false)) {
            ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
        }
    }

    // Display metadata status with appropriate security policy
    if (certificate_data->poolRegistration.poolMetadataIsNull) {
        security_policy_t no_metadata_policy = policyForSignTxStakePoolRegistrationNoMetadata();
        LEDGER_ASSERT(no_metadata_policy != POLICY_DENY, "No metadata security policy denied");

        if (no_metadata_policy == POLICY_SHOW) {
            if (!ui_pairs_add_static_label_impl(UI_STATIC_LABEL("Metadata"), (char *) UI_STATIC_LABEL("none (anonymous pool)"), false)) {
                ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
            }
        }
    } else {
        security_policy_t metadata_policy = policyForSignTxStakePoolRegistrationMetadata();
        LEDGER_ASSERT(metadata_policy != POLICY_DENY, "Metadata security policy denied");

        if (metadata_policy == POLICY_SHOW) {
            size_t url_size = certificate_data->poolRegistration.poolMetadata.urlSize;
            char *metadata_url = (char *) app_mem_alloc(url_size + 2);
            if (metadata_url == NULL) {
                ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
            } else {
                memcpy(metadata_url,
                        certificate_data->poolRegistration.poolMetadata.url,
                        url_size);
                metadata_url[url_size] = '\0';
                LEDGER_ASSERT(strlen(metadata_url) <= url_size, "Pool metadata url ui string buffer too short");
                if (!ui_pairs_add_static_label(UI_STATIC_LABEL("Pool metadata url"), metadata_url)) {
                    ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
                } else {
                    UI_ADD_FORMAT2(UI_STATIC_LABEL("Pool metadata hash"), MAX_POOL_METADATA_HASH_STRING_LENGTH, format_hex_ui, certificate_data->poolRegistration.poolMetadata.hash, POOL_METADATA_HASH_LENGTH);
                }
            }
        }
    }
}

static void ui_strings_certificates(transaction_t *tx) {
    uint16_t certificate_num = 1;
    s_flist_node *certificate_node = tx->certificates;
    TRACE("Materializing %u certificates", tx->num_certificates);
    while (certificate_node != NULL) {
        tx_certificate_list_item_t *certificate_item =
            (tx_certificate_list_item_t *) certificate_node;
        s_flist_node *next = certificate_node->next;

        // Determine security policy based on certificate type
        security_policy_t policy = POLICY_DENY;
        switch (certificate_item->certificate_data.type) {
            case CERTIFICATE_STAKE_REGISTRATION:
            case CERTIFICATE_STAKE_DEREGISTRATION:
            case CERTIFICATE_STAKE_REGISTRATION_CONWAY:
            case CERTIFICATE_STAKE_DEREGISTRATION_CONWAY:
            case CERTIFICATE_STAKE_DELEGATION:
                policy = policyForSignTxCertificateStaking(
                    tx->txSigningMode,
                    certificate_item->certificate_data.type,
                    &certificate_item->certificate_data.stakeCredential
                );
                break;

            case CERTIFICATE_VOTE_DELEGATION:
                policy = policyForSignTxCertificateVoteDelegation(
                    tx->txSigningMode,
                    &certificate_item->certificate_data.stakeCredential,
                    &certificate_item->certificate_data.drep
                );
                break;

            case CERTIFICATE_AUTHORIZE_COMMITTEE_HOT:
                policy = policyForSignTxCertificateCommitteeAuth(
                    tx->txSigningMode,
                    &certificate_item->certificate_data.coldCredential,
                    &certificate_item->certificate_data.hotCredential
                );
                break;

            case CERTIFICATE_RESIGN_COMMITTEE_COLD:
                policy = policyForSignTxCertificateCommitteeResign(
                    tx->txSigningMode,
                    &certificate_item->certificate_data.coldCredential
                );
                break;

            case CERTIFICATE_DREP_REGISTRATION:
            case CERTIFICATE_DREP_DEREGISTRATION:
            case CERTIFICATE_DREP_UPDATE:
                policy = policyForSignTxCertificateDRep(
                    tx->txSigningMode,
                    &certificate_item->certificate_data.dRepCredential
                );
                break;

            case CERTIFICATE_STAKE_POOL_RETIREMENT:
                policy = policyForSignTxCertificateStakePoolRetirement(
                    tx->txSigningMode,
                    &certificate_item->certificate_data.poolCredential,
                    certificate_item->certificate_data.retirementEpoch
                );
                break;

            case CERTIFICATE_STAKE_POOL_REGISTRATION:
                {
                    pool_owner_counts_t pool_owner_counts = count_pool_owner_nodes(
                        certificate_item->certificate_data.poolRegistration.poolOwners
                    );
                    policy = policyForSignTxStakePoolRegistrationInit(
                        tx->txSigningMode,
                        certificate_item->certificate_data.poolRegistration.numPoolOwners,
                        pool_owner_counts.path_owners
                    );
                }
                break;

            default:
                LEDGER_ASSERT(false, "Unknown certificate type");
                policy = POLICY_DENY;
                break;
        }

        switch (policy) {
            case POLICY_DENY:
                LEDGER_ASSERT(false, "Certificate denied during UI");
                break;
            case POLICY_SHOW: {
                TRACE("Materializing certificate #%u type=%u", certificate_num, certificate_item->certificate_data.type);
                UI_ADD_FORMAT1(UI_STATIC_LABEL("Certificate"), MAX_UINT64_STRING_LENGTH, format_index_with_prefix, certificate_num);
                UI_ADD_FORMAT1(UI_STATIC_LABEL("Type"), MAX_CERTIFICATE_TYPE_LENGTH, format_certificate_type, certificate_item->certificate_data.type);

                // Certificate-specific fields
                switch (certificate_item->certificate_data.type) {
                    case CERTIFICATE_STAKE_REGISTRATION:
                    case CERTIFICATE_STAKE_DEREGISTRATION: {
                        ui_strings_certificate_stake_registration(&certificate_item->certificate_data);
                        break;
                    }

                    case CERTIFICATE_STAKE_DELEGATION: {
                        ui_strings_certificate_stake_delegation(&certificate_item->certificate_data);
                        break;
                    }

                    case CERTIFICATE_STAKE_REGISTRATION_CONWAY:
                    case CERTIFICATE_STAKE_DEREGISTRATION_CONWAY: {
                        ui_strings_certificate_stake_conway(&certificate_item->certificate_data);
                        break;
                    }

                    case CERTIFICATE_STAKE_POOL_RETIREMENT: {
                        ui_strings_certificate_pool_retirement(&certificate_item->certificate_data);
                        break;
                    }

                    case CERTIFICATE_VOTE_DELEGATION: {
                        ui_strings_certificate_vote_delegation(&certificate_item->certificate_data);
                        break;
                    }

                    case CERTIFICATE_AUTHORIZE_COMMITTEE_HOT: {
                        ui_strings_certificate_committee_hot(&certificate_item->certificate_data);
                        break;
                    }

                    case CERTIFICATE_RESIGN_COMMITTEE_COLD: {
                        ui_strings_certificate_committee_resign(&certificate_item->certificate_data);
                        break;
                    }

                    case CERTIFICATE_DREP_REGISTRATION: {
                        ui_strings_certificate_drep_registration(&certificate_item->certificate_data);
                        break;
                    }

                    case CERTIFICATE_DREP_DEREGISTRATION: {
                        ui_strings_certificate_drep_deregistration(&certificate_item->certificate_data);
                        break;
                    }

                    case CERTIFICATE_DREP_UPDATE: {
                        ui_strings_certificate_drep_update(&certificate_item->certificate_data);
                        break;
                    }

                    case CERTIFICATE_STAKE_POOL_REGISTRATION: {
                        ui_strings_certificate_pool_registration(&certificate_item->certificate_data, tx->txSigningMode);
                        break;
                    }

                    default:
                        LEDGER_ASSERT(false, "Unknown certificate type");
                }

                certificate_num++;
                break;
            }
            case POLICY_HIDE:
                break;
        }

        app_mem_free(certificate_item);
        certificate_node = next;
    }
    tx->certificates = NULL;
}

static void ui_strings_withdrawals(transaction_t *tx) {
    uint16_t withdrawal_num = 1;
    s_flist_node *withdrawal_node = tx->withdrawals;
    TRACE("Materializing %u withdrawals", tx->num_withdrawals);
    while (withdrawal_node != NULL) {
        tx_withdrawal_list_item_t *withdrawal_item =
            (tx_withdrawal_list_item_t *) withdrawal_node;
        s_flist_node *next = withdrawal_node->next;

        security_policy_t policy = policyForSignTxWithdrawal(
            tx->txSigningMode,
            &withdrawal_item->withdrawal_data.stakeCredential,
            &G_context.tx_info.warning_bits
        );
        LEDGER_ASSERT(policy != POLICY_DENY, "Withdrawal denied during UI");

        switch (policy) {
            case POLICY_DENY:
                // Already asserted above, this case should never be reached
                break;
            case POLICY_SHOW: {
                TRACE("Materializing withdrawal #%u", withdrawal_num);
                UI_ADD_FORMAT1(UI_STATIC_LABEL("Withdrawal"), MAX_UINT64_STRING_LENGTH, format_index_with_prefix, withdrawal_num);
                UI_ADD_FORMAT1(UI_STATIC_LABEL("Amount"), MAX_ADA_AMOUNT_STRING_LENGTH, str_formatAdaAmount, withdrawal_item->withdrawal_data.amount);

                addRewardAccountFromCredentialUIPairs(
                    G_context.tx_info.transaction.networkId,
                    &withdrawal_item->withdrawal_data.stakeCredential
                );
                withdrawal_num++;
            }
            break;
            case POLICY_HIDE:
                break;
        }

        app_mem_free(withdrawal_item);
        withdrawal_node = next;
    }
    tx->withdrawals = NULL;
}

static void ui_strings_aux_data_hash(transaction_t *tx) {
    if (tx->includeAuxDataHash) {
        security_policy_t policy = policyForSignTxAuxData(tx->auxDataType);
        LEDGER_ASSERT(policy != POLICY_DENY, "Aux data denied during UI");
        if (policy == POLICY_SHOW) {
            UI_ADD_FORMAT2(UI_STATIC_LABEL("Auxiliary data hash"), MAX_TX_HASH_DISPLAY_LENGTH, format_hex_ui, tx->auxDataHash, AUX_DATA_HASH_LENGTH);
        }
    }
}

static void ui_strings_validity_interval_start(transaction_t *tx) {
    if (!tx->includeValidityIntervalStart) {
        return;
    }
    security_policy_t validity_interval_start_policy = policyForSignTxValidityIntervalStart();
    LEDGER_ASSERT(validity_interval_start_policy != POLICY_DENY, "Validity interval start denied during UI");
    switch (validity_interval_start_policy) {
        case POLICY_DENY:
            // Already asserted above, this case should never be reached
            break;
        case POLICY_SHOW:
            UI_ADD_FORMAT3(UI_STATIC_LABEL("Validity interval start"), MAX_VALIDITY_BOUNDARY_STRING_LENGTH, str_formatValidityBoundary, tx->validityIntervalStart, tx->networkId, tx->protocolMagic);
            break;
        case POLICY_HIDE:
            break;
    }
}

static void ui_strings_mint(transaction_t *tx) {
    if (tx->num_mint_asset_groups > 0) {
        security_policy_t mint_policy = policyForSignTxMintInit(tx->txSigningMode);
        LEDGER_ASSERT(mint_policy != POLICY_DENY, "Mint denied during UI");
        if (mint_policy == POLICY_SHOW) {
            char *summary_tmp = (char *) app_mem_alloc(MAX_MINT_SUMMARY_STRING_LENGTH + UI_BUFFER_SAFETY_MARGIN);
            if (summary_tmp == NULL) {
                ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
            } else {
                snprintf(summary_tmp,
                         MAX_MINT_SUMMARY_STRING_LENGTH + UI_BUFFER_SAFETY_MARGIN,
                         "%u asset group%s",
                         tx->num_mint_asset_groups,
                         (tx->num_mint_asset_groups == 1) ? "" : "s");
                LEDGER_ASSERT(strlen(summary_tmp) <= MAX_MINT_SUMMARY_STRING_LENGTH, "Mint summary ui string buffer too short");
                if (ui_pairs_add_static_label(UI_STATIC_LABEL("Mint"), summary_tmp)) {
                    token_group_t tokenGroup;
                    s_flist_node *mint_node = tx->mint_asset_groups;
                    while (mint_node != NULL) {
                        mint_asset_group_list_item_t *item =
                            (mint_asset_group_list_item_t *) mint_node;
                        s_flist_node *mint_next = mint_node->next;
                        memcpy(tokenGroup.policyId,
                               item->asset_group.policyId,
                               sizeof(tokenGroup.policyId));

                        if (item->asset_group.tokens == NULL) {
                            mint_node = mint_node->next;
                            continue;
                        }

                        // Iterate through linked list of tokens
                        s_flist_node *token_node = item->asset_group.tokens;
                        while (token_node != NULL) {
                            mint_token_list_item_t *token_item = (mint_token_list_item_t *) token_node;
                            mint_token_t *token = &token_item->token_data;
                            s_flist_node *token_next = token_node->next;

                            UI_ADD_FORMAT3(UI_STATIC_LABEL("Mint fingerprint"), MAX_TOKEN_FINGERPRINT_STRING_LENGTH, format_asset_fingerprint_bech32, &tokenGroup, token->assetName, token->assetNameLen);
                            UI_ADD_FORMAT4(UI_STATIC_LABEL("Mint amount"), MAX_MINT_AMOUNT_STRING_LENGTH, format_token_amount_mint, &tokenGroup, token->assetName, token->assetNameLen, token->amount);

                            // Free token node immediately after UI strings are formatted
                            app_mem_free(token_node);
                            token_node = token_next;
                        }
                        item->asset_group.tokens = NULL;

                        app_mem_free(item);
                        mint_node = mint_next;
                    }
                    tx->mint_asset_groups = NULL;
                } else {
                    ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
                }
            }
        }
    }
    if (tx->num_mint_asset_groups > 0 && tx->mint_asset_groups != NULL) {
        s_flist_node *mint_node = tx->mint_asset_groups;
        while (mint_node != NULL) {
            mint_asset_group_list_item_t *item =
                (mint_asset_group_list_item_t *) mint_node;
            s_flist_node *mint_next = mint_node->next;
            s_flist_node *token_node = item->asset_group.tokens;
            while (token_node != NULL) {
                s_flist_node *token_next = token_node->next;
                app_mem_free(token_node);
                token_node = token_next;
            }
            item->asset_group.tokens = NULL;
            app_mem_free(item);
            mint_node = mint_next;
        }
        tx->mint_asset_groups = NULL;
    }
}

static void ui_strings_script_data_hash(transaction_t *tx) {
    if (tx->includeScriptDataHash) {
        security_policy_t policy = policyForSignTxScriptDataHash(tx->txSigningMode);
        LEDGER_ASSERT(policy != POLICY_DENY, "Script data hash denied during UI");
        if (policy == POLICY_SHOW) {
            UI_ADD_FORMAT2(UI_STATIC_LABEL("Script data hash"), MAX_TX_HASH_DISPLAY_LENGTH, format_hex_ui, tx->scriptDataHash, SCRIPT_DATA_HASH_LENGTH);
        }
    }
}

static void ui_strings_collateral_inputs(transaction_t *tx) {
    if (tx->num_collateral_inputs == 0) {
        return;
    }

    s_flist_node *collateral_input_node = tx->collateral_inputs;
    while (collateral_input_node != NULL) {
        tx_collateral_input_list_item_t *input_item =
            (tx_collateral_input_list_item_t *) collateral_input_node;
        s_flist_node *next = collateral_input_node->next;

        security_policy_t collateral_input_policy = policyForSignTxCollateralInput(
            tx->txSigningMode,
            tx->includeTotalCollateral,
            &input_item->input_data);
        LEDGER_ASSERT(collateral_input_policy != POLICY_DENY, "Collateral input policy denied during UI");

        if (collateral_input_policy == POLICY_SHOW) {
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Coll input"), MAX_INPUT_DISPLAY_STRING_LENGTH, format_input_with_index, &input_item->input_data);
        }

        app_mem_free(input_item);
        collateral_input_node = next;
    }
    tx->collateral_inputs = NULL;
}

static void ui_strings_required_signers(transaction_t *tx) {
    if (tx->num_required_signers > 0) {
        s_flist_node *req_signer_node = tx->required_signers;
        while (req_signer_node != NULL) {
            tx_required_signer_list_item_t *item = (tx_required_signer_list_item_t *) req_signer_node;
            s_flist_node *next = req_signer_node->next;

            security_policy_t policy = policyForSignTxRequiredSigner(tx->txSigningMode, &item->required_signer_data);
            LEDGER_ASSERT(policy != POLICY_DENY, "Required signer denied during UI");

            if (policy == POLICY_SHOW) {
                switch (item->required_signer_data.type) {
                    case REQUIRED_SIGNER_WITH_HASH: {
                        UI_ADD_FORMAT3(UI_STATIC_LABEL("Required signer"), MAX_BECH32_STRING_LENGTH, format_bech32, "vkh", item->required_signer_data.keyHash, ADDRESS_KEY_HASH_LENGTH);
                        break;
                    }
                    case REQUIRED_SIGNER_WITH_PATH: {
                        UI_ADD_FORMAT1(UI_STATIC_LABEL("Required signer"), MAX_BIP44_PATH_STRING_LENGTH, format_bip44_path, &item->required_signer_data.keyPath);
                        break;
                    }
                    default:
                        LEDGER_ASSERT(false, "Unknown required signer type");
                }
            }
            app_mem_free(item);
            req_signer_node = next;
        }
    }
    tx->required_signers = NULL;
}

static void ui_strings_collateral_output(transaction_t *tx) {
    if (!tx->includeCollateralOutput) {
        return;
    }
    tx_output_description_t collateral_desc = {
        .format = tx->collateral_output.format,
        .amount = tx->collateral_output.adaAmount,
        .numAssetGroups = tx->collateral_output.numAssetGroups,
        .includeDatum = tx->collateral_output.datum.hasDatum,
        .includeRefScript = tx->collateral_output.hasRefScript,
    };

    if (tx->collateral_output.destination.type == DESTINATION_THIRD_PARTY) {
        collateral_desc.destination.type = DESTINATION_THIRD_PARTY;
        collateral_desc.destination.address.buffer =
            tx->collateral_output.destination.address.buffer;
        collateral_desc.destination.address.size =
            tx->collateral_output.destination.address.size;
    } else {
        collateral_desc.destination.type = DESTINATION_DEVICE_OWNED;
        collateral_desc.destination.params = &tx->collateral_output.destination.params;
    }

    security_policy_t collateral_policy =
        (collateral_desc.destination.type == DESTINATION_THIRD_PARTY)
            ? policyForSignTxCollateralOutputAddressBytes(
                &collateral_desc,
                tx->txSigningMode,
                tx->networkId,
                tx->protocolMagic)
            : policyForSignTxCollateralOutputAddressParams(
                &collateral_desc,
                tx->txSigningMode,
                tx->networkId,
                tx->protocolMagic,
                tx->includeTotalCollateral);
    LEDGER_ASSERT(collateral_policy != POLICY_DENY, "Collateral output denied during UI");

    security_policy_t collateral_ada_policy =
        policyForSignTxCollateralOutputAdaAmount(collateral_policy, tx->includeTotalCollateral);
    LEDGER_ASSERT(collateral_ada_policy != POLICY_DENY, "Collateral ADA policy denied during UI");
    security_policy_t collateral_tokens_policy =
        policyForSignTxCollateralOutputTokens(collateral_policy, &collateral_desc);
    LEDGER_ASSERT(collateral_tokens_policy != POLICY_DENY, "Collateral tokens policy denied during UI");
    security_policy_t collateral_confirm_policy =
        policyForSignTxCollateralOutputConfirm(collateral_policy, collateral_desc.numAssetGroups);
    LEDGER_ASSERT(collateral_confirm_policy != POLICY_DENY, "Collateral confirm policy denied during UI");

    bool show_collateral_tokens =
        (collateral_policy == POLICY_SHOW) && (collateral_tokens_policy == POLICY_SHOW);
    TRACE("Collateral output: policy=%d ada=%d tokens=%d numAssets=%u",
          collateral_policy, collateral_ada_policy, collateral_tokens_policy,
          (unsigned int)tx->collateral_output.numAssetGroups);

    if (collateral_policy == POLICY_SHOW) {
        UI_ADD_FORMAT1(UI_STATIC_LABEL("Collateral address"), MAX_HUMAN_ADDRESS_LENGTH, format_output_address, &collateral_desc);

        // For device-owned collateral addresses, show payment and staking details
        if (collateral_desc.destination.type == DESTINATION_DEVICE_OWNED) {
            addPaymentInfoUIPair(collateral_desc.destination.params);
            addStakingInfoUIPair(collateral_desc.destination.params);
        }

        if (collateral_ada_policy == POLICY_SHOW) {
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Collateral amount"), MAX_ADA_AMOUNT_STRING_LENGTH, str_formatAdaAmount, collateral_desc.amount);
        }

        if (collateral_confirm_policy == POLICY_SHOW) {
            warning_bits_set(&G_context.tx_info.warning_bits, WARNING_BIT_COLLATERAL_OUTPUT_WARNING);
        }
    }

    ui_format_token_groups(
        tx->collateral_output.assetGroups,
        tx->collateral_output.numAssetGroups,
        show_collateral_tokens);

    if (tx->collateral_output.assetGroups != NULL) {
        for (uint16_t ag = 0; ag < tx->collateral_output.numAssetGroups; ag++) {
            s_flist_node *token_node = tx->collateral_output.assetGroups[ag].tokens;
            while (token_node != NULL) {
                s_flist_node *token_next = token_node->next;
                app_mem_free(token_node);
                token_node = token_next;
            }
            tx->collateral_output.assetGroups[ag].tokens = NULL;
        }
        app_mem_free(tx->collateral_output.assetGroups);
        tx->collateral_output.assetGroups = NULL;
    }
}

static void ui_strings_total_collateral(transaction_t *tx) {
    if (!tx->includeTotalCollateral) {
        return;
    }
    security_policy_t policy = policyForSignTxTotalCollateral();
    LEDGER_ASSERT(policy != POLICY_DENY, "Total collateral denied during UI");
    if (policy == POLICY_SHOW) {
        UI_ADD_FORMAT1(UI_STATIC_LABEL("Total collateral"), MAX_ADA_AMOUNT_STRING_LENGTH, str_formatAdaAmount, tx->totalCollateral);
    }
}

static void ui_strings_reference_inputs(transaction_t *tx) {
    if (tx->num_reference_inputs == 0) {
        return;
    }

    s_flist_node *reference_input_node = tx->reference_inputs;
    while (reference_input_node != NULL) {
        tx_input_list_item_t *input_item = (tx_input_list_item_t *) reference_input_node;
        s_flist_node *next = reference_input_node->next;

        security_policy_t reference_input_policy = policyForSignTxReferenceInput(
            tx->txSigningMode,
            &input_item->input_data);
        LEDGER_ASSERT(reference_input_policy != POLICY_DENY, "Reference input denied during UI");

        if (reference_input_policy == POLICY_SHOW) {
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Ref input"), MAX_INPUT_DISPLAY_STRING_LENGTH, format_input_with_index, &input_item->input_data);
        }

        app_mem_free(input_item);
        reference_input_node = next;
    }
    tx->reference_inputs = NULL;
}

static void ui_strings_voting_procedures(transaction_t *tx) {
    if (tx->num_voters > 0) {
        s_flist_node *voter_node = tx->voting_procedures;
        while (voter_node != NULL) {
            voter_votes_list_item_t *voter_item = (voter_votes_list_item_t *) voter_node;
            s_flist_node *voter_next = voter_node->next;

            security_policy_t policy = policyForSignTxVotingProcedure(tx->txSigningMode, &voter_item->voter_votes_data.voter);
            LEDGER_ASSERT(policy != POLICY_DENY, "Voting procedure denied during UI");

            if (policy == POLICY_SHOW) {
                // Display Voter
                addVoterUIPairs(&voter_item->voter_votes_data.voter);
                // Iterate votes
                s_flist_node *vote_node = voter_item->voter_votes_data.votes;
                while (vote_node != NULL) {
                    vote_list_item_t *vote_item = (vote_list_item_t *) vote_node;

                    // Gov Action Tx Hash
                    UI_ADD_FORMAT2(UI_STATIC_LABEL("Gov action tx hash"), MAX_TX_HASH_DISPLAY_LENGTH, format_hex_ui, vote_item->vote_data.govActionId.txHash, TX_HASH_LENGTH);

                    // Gov Action Index
                    UI_ADD_FORMAT1(UI_STATIC_LABEL("Gov action index"), MAX_UINT64_STRING_LENGTH, format_u64_ui, vote_item->vote_data.govActionId.govActionIndex);

                    // Vote Option
                    UI_ADD_FORMAT1(UI_STATIC_LABEL("Vote"), MAX_VOTE_OPTION_LENGTH, format_vote_option, vote_item->vote_data.voteOption);

                    // Anchor
                    addAnchorUIPairs(&vote_item->vote_data.anchor);

                    vote_node = vote_node->next;
                }
            }

            s_flist_node *vote_node = voter_item->voter_votes_data.votes;
            while (vote_node != NULL) {
                s_flist_node *vote_next = vote_node->next;
                app_mem_free(vote_node);
                vote_node = vote_next;
            }
            voter_item->voter_votes_data.votes = NULL;
            app_mem_free(voter_item);
            voter_node = voter_next;
        }
    }
    tx->voting_procedures = NULL;
}

static void ui_strings_treasury(transaction_t *tx) {
    if (tx->includeTreasury) {
        security_policy_t policy = policyForSignTxTreasury(tx->txSigningMode, tx->treasury);
        LEDGER_ASSERT(policy != POLICY_DENY, "Treasury denied during UI");
        if (policy == POLICY_SHOW) {
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Treasury"), MAX_ADA_AMOUNT_STRING_LENGTH, str_formatAdaAmount, tx->treasury);
        }
    }
}

static void ui_strings_donation(transaction_t *tx) {
    if (tx->includeDonation) {
        security_policy_t policy = policyForSignTxDonation(tx->txSigningMode, tx->donation);
        LEDGER_ASSERT(policy != POLICY_DENY, "Donation denied during UI");
        if (policy == POLICY_SHOW) {
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Donation"), MAX_ADA_AMOUNT_STRING_LENGTH, str_formatAdaAmount, tx->donation);
        }
    }
}

static void ui_strings_tx_hash(void) {
    if (G_context.tx_info.raw_tx != NULL) {
        app_mem_free(G_context.tx_info.raw_tx);
        G_context.tx_info.raw_tx = NULL;
        G_context.tx_info.raw_tx_len = 0;
    }

    UI_ADD_FORMAT2(UI_STATIC_LABEL("Transaction hash"), MAX_TX_HASH_DISPLAY_LENGTH, format_hex_ui, G_context.tx_info.tx_hash, sizeof(G_context.tx_info.tx_hash));
}

static int add_ui_strings_and_free_parsed_data(void) {
    LEDGER_ASSERT(G_context.state.tx_state == TX_STATE_HASHED, "String formatting invoked too early");
    transaction_t *tx = &G_context.tx_info.transaction;

    // Initialize error status before formatting
    ui_reset_error_status();

    TRACE("UI formatting starting");

    ui_strings_inputs(tx);
    ui_strings_outputs(tx);
    ui_strings_fee(tx);
    ui_strings_ttl(tx);
    ui_strings_certificates(tx);
    ui_strings_withdrawals(tx);
    ui_strings_aux_data_hash(tx);
    ui_strings_validity_interval_start(tx);
    ui_strings_mint(tx);
    ui_strings_script_data_hash(tx);
    ui_strings_collateral_inputs(tx);
    ui_strings_required_signers(tx);
    ui_strings_collateral_output(tx);
    ui_strings_total_collateral(tx);
    ui_strings_reference_inputs(tx);
    ui_strings_voting_procedures(tx);
    ui_strings_treasury(tx);
    ui_strings_donation(tx);
    ui_strings_tx_hash();

    TRACE("UI formatting complete");
    ui_status_t status = ui_get_error_status();
    switch (status) {
        case UI_STATUS_SUCCESS:
            return SWO_SUCCESS;
        case UI_STATUS_OUT_OF_MEMORY:
            return SWO_INSUFFICIENT_MEMORY;
        case UI_STATUS_UNINITIALIZED:
        default:
            ASSERT(false);
            return SWO_BAD_STATE;
    }
}

static inline bool status_requires_streaming(int status) {
    return status == SWO_INSUFFICIENT_MEMORY;
}

static int ui_build_pairs_and_warnings(void) {
    int status = add_ui_strings_and_free_parsed_data();
    if (status != SWO_SUCCESS) {
        return status;
    }
    ui_status_t warning_status = ui_build_warnings(G_context.tx_info.warning_bits);
    switch (warning_status) {
        case UI_STATUS_SUCCESS:
            return SWO_SUCCESS;
        case UI_STATUS_OUT_OF_MEMORY:
            return SWO_INSUFFICIENT_MEMORY;
        case UI_STATUS_UNINITIALIZED:
        default:
            ASSERT(false);
            return SWO_BAD_STATE;
    }
}

int ui_prepare_transaction_review(void) {
    if (G_context.req_type != REQUEST_SIGN_TRANSACTION) {
        return send_swo_and_reset(SWO_BAD_STATE);
    }
    LEDGER_ASSERT(G_context.state.tx_state == TX_STATE_HASHED, "UI prep called too early");
    uint16_t pair_count = G_context.tx_info.planned_ui_pairs;

    // pair_count should never be 0 - at minimum we display fee
    LEDGER_ASSERT(pair_count > 0, "UI pair count is zero - at minimum fee must be displayed");

    // If pair count exceeds UI capability, reject the transaction
    if (pair_count > MAX_UI_PAIRS) {
        return send_swo_and_reset(SWO_UI_PAIRS_EXCEED_CAPABILITY);
    }

    if (!ui_pairs_init((uint8_t) pair_count)) {
        return send_swo_and_reset(SWO_INSUFFICIENT_MEMORY);
    }

    int status = ui_build_pairs_and_warnings();
    if (status != SWO_SUCCESS) {
        ui_pairs_cleanup();
        ui_clear_warnings();
        if (status_requires_streaming(status)) {
            LEDGER_ASSERT(false, "Need streaming UI but not implemented (status=0x%04x)", status);
        }
        return status;
    }

    // Validate that the actual number of pairs formatted matches the planned count
    LEDGER_ASSERT(ui_pairs_get_count() == pair_count,
                  "UI pair count mismatch: planned %u but formatted %u",
                  pair_count, ui_pairs_get_count());

    return SWO_SUCCESS;
}
