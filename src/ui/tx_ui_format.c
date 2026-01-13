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
 * - tx_ui_format.c: calls `addPaymentInfoUIPairs()` + `addStakingInfoUIPairs()` (line 263, 1655)
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
#include "addressUtils/bip44.h"
#include "memory/mem.h"
#include "securityPolicy/securityPolicy.h"
#include "securityPolicy/securityWarnings.h"
#include "transaction/tx_utils.h"
#include "utils/assert.h"
#include "utils/ipUtils.h"
#include "ui/ui_utils.h"
#include "ui/ui_formatters.h"
#include "ui/ui_warnings.h"
#include "ui/ui_display_tx.h"
#include "ui/tx_ui_helpers.h"
#include "io.h"
#include "app_context.h"
#include "cardano_tokens/cardano_tokens.h"
#include "transaction/tx_voting_procedure_types.h"
#include "addressUtils/bech32.h"

// Max display lengths
#define MAX_DATUM_HASH_STRING_LENGTH (2 * OUTPUT_DATUM_HASH_LENGTH + 1)
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

static void ui_strings_inputs(transaction_t *tx) {
    s_flist_node *node = tx->inputs;
    while (node != NULL) {
        tx_input_node_t *input_node = (tx_input_node_t *) node;
        security_policy_t input_policy = policyForSignTxInput(tx->txSigningMode, &input_node->input);
        LEDGER_ASSERT(input_policy != POLICY_DENY, "Input denied during UI");

        if (input_policy == POLICY_SHOW) {
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Input"), MAX_INPUT_DISPLAY_STRING_LENGTH, format_input_with_index, &input_node->input);
        }

        node = node->next;
        app_mem_free(input_node); // only after next is assigned
    }
    tx->inputs = NULL;
}


static void format_and_free_output_token_nodes(const output_asset_group_t *group,
                                               s_flist_node *token_nodes_start,
                                               bool show_tokens) {
    s_flist_node *node = token_nodes_start;
    while (node != NULL) {
        output_token_node_t *token_node = (output_token_node_t *) node;
        output_token_t *token = &token_node->token_data;

        if (show_tokens) {
            UI_ADD_FORMAT3(UI_STATIC_LABEL("Asset fingerprint"), MAX_TOKEN_FINGERPRINT_STRING_LENGTH, format_asset_fingerprint_bech32, group->policyId, token->assetName, token->assetNameLen);
            UI_ADD_FORMAT4(UI_STATIC_LABEL("Token amount"), MAX_TOKEN_AMOUNT_OUTPUT_STRING_LENGTH, format_token_amount_output, group->policyId, token->assetName, token->assetNameLen, token->amount);
        }

        node = node->next;
        app_mem_free(token_node);
    }
}

static void ui_format_and_free_output_asset_groups(s_flist_node* assetGroupNodes,
                                            uint16_t numGroups,
                                            bool show_tokens) {
    uint16_t group_count = 0;
    s_flist_node *node = assetGroupNodes;
    while (node != NULL) {
        group_count++;
        output_asset_group_node_t *group_node = (output_asset_group_node_t *) node;
        output_asset_group_t *group = &group_node->asset_group;
        format_and_free_output_token_nodes(group, group->tokens, show_tokens);
        group->tokens = NULL;

        node = node->next;
        app_mem_free(group_node);
    }
    LEDGER_ASSERT(group_count == numGroups, "Output asset group count mismatch");
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

// TODO this needs revision, does not follow conventions and might not need tx_output_description_t?
static void ui_strings_outputs(transaction_t *tx) {
    uint16_t output_num = 1;
    s_flist_node *node = tx->outputs;
    TRACE("Formatting %u outputs", tx->num_outputs);
    while (node != NULL) {
        tx_output_node_t *output_node = (tx_output_node_t *) node;

        tx_output_description_t output_desc = {
            .format = output_node->output_data.format,
            .amount = output_node->output_data.adaAmount,
            .numAssetGroups = output_node->output_data.numAssetGroups,
            .includeDatum = output_node->output_data.datum.hasDatum,
            .includeRefScript = output_node->output_data.refScript.hasRefScript,
        };

        if (output_node->output_data.destination.type == DESTINATION_THIRD_PARTY) {
            output_desc.destination.type = DESTINATION_THIRD_PARTY;
            output_desc.destination.address.buffer = output_node->output_data.destination.address.buffer;
            output_desc.destination.address.size = output_node->output_data.destination.address.size;
        } else {
            output_desc.destination.type = DESTINATION_DEVICE_OWNED;
            output_desc.destination.params = &output_node->output_data.destination.params;
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

        switch (policy) {
            case POLICY_DENY:
                LEDGER_ASSERT(false, "Output denied during UI");
                break;
            case POLICY_SHOW: {
                TRACE("Formatting output #%u", output_num);
                UI_ADD_FORMAT1(UI_STATIC_LABEL("Output"), MAX_UINT64_STRING_LENGTH, format_index_with_prefix, output_num);
                UI_ADD_FORMAT1(UI_STATIC_LABEL("Address"), MAX_HUMAN_ADDRESS_LENGTH, format_output_address, &output_desc);

                // For device-owned addresses, show payment and staking details
                if (output_node->output_data.destination.type == DESTINATION_DEVICE_OWNED) {
                    addPaymentInfoUIPairs(&output_node->output_data.destination.params);
                    addStakingInfoUIPairs(&output_node->output_data.destination.params);
                }

                UI_ADD_FORMAT1(UI_STATIC_LABEL("Amount"), MAX_ADA_AMOUNT_STRING_LENGTH, format_ada_amount, output_node->output_data.adaAmount);

                if (output_node->output_data.datum.hasDatum) {
                    security_policy_t datum_policy = policyForSignTxOutputDatumHash(policy);
                    LEDGER_ASSERT(datum_policy != POLICY_DENY, "Output datum policy denied during UI");
                    if (datum_policy == POLICY_SHOW) {
                        if (output_node->output_data.datum.type == DATUM_HASH) {
                            UI_ADD_FORMAT2(UI_STATIC_LABEL("Datum hash"), MAX_DATUM_HASH_STRING_LENGTH, format_hex_bytes, output_node->output_data.datum.hash, OUTPUT_DATUM_HASH_LENGTH);
                        } else {
                            // TODO: Inline datum size is not bounded by protocol; handle large values more robustly.
                            UI_ADD_FORMAT2(UI_STATIC_LABEL("Inline datum"), MAX_INLINE_DATUM_STRING_LENGTH, format_hex_bytes, output_node->output_data.datum.inline_data.data, output_node->output_data.datum.inline_data.size);
                        }
                    }
                }

                if (output_node->output_data.refScript.hasRefScript) {
                    security_policy_t ref_script_policy = policyForSignTxOutputRefScript(policy);
                    LEDGER_ASSERT(ref_script_policy != POLICY_DENY, "Output ref script policy denied during UI");
                    if (ref_script_policy == POLICY_SHOW) {
                        // TODO: Reference script size is not bounded by protocol; handle large values more robustly.
                        UI_ADD_FORMAT2(UI_STATIC_LABEL("Reference script"), MAX_REFERENCE_SCRIPT_STRING_LENGTH, format_hex_bytes, output_node->output_data.refScript.data, output_node->output_data.refScript.size);
                    }
                }

                if (output_node->output_data.assetGroups != NULL) {
                    ui_format_and_free_output_asset_groups(
                        output_node->output_data.assetGroups,
                        output_node->output_data.numAssetGroups,
                        true
                    );
                    output_node->output_data.assetGroups = NULL;
                }

                output_num++;
                break;
            }
            case POLICY_HIDE:
                break;
        }

        if (output_node->output_data.assetGroups != NULL) {
            ui_format_and_free_output_asset_groups(
                output_node->output_data.assetGroups,
                output_node->output_data.numAssetGroups,
                false
            );
            output_node->output_data.assetGroups = NULL;
        }
        // Note: inline datum and reference script data are pointers into the raw_tx buffer,
        // not separately allocated, so they do not need to be freed
        node = node->next;
        app_mem_free(output_node);
    }
    tx->outputs = NULL;
}

static void ui_strings_fee(transaction_t *tx) {
    UI_ADD_FORMAT1(UI_STATIC_LABEL("Fee"), MAX_ADA_AMOUNT_STRING_LENGTH, format_ada_amount, tx->fee);
}

static void ui_strings_ttl(transaction_t *tx) {
    if (!tx->includeTtl) {
        return;
    }
    security_policy_t ttl_policy = policyForSignTxTtl(tx->ttl);
    switch (ttl_policy) {
        case POLICY_DENY:
            LEDGER_ASSERT(false, "TTL denied during UI");
            break;
        case POLICY_SHOW:
            UI_ADD_FORMAT3(UI_STATIC_LABEL("TTL"), MAX_VALIDITY_BOUNDARY_STRING_LENGTH, format_validity_boundary, tx->ttl, tx->networkId, tx->protocolMagic);
            break;
        case POLICY_HIDE:
            break;
    }
}

static void ui_strings_certificate_pool_registration(const certificate_data_t* certificate_data,
                                                     sign_tx_signingmode_t txSigningMode) {
    pool_owner_counts_t pool_owner_counts = count_pool_owner_nodes(
        certificate_data->poolRegistration.poolOwners
    );

    security_policy_t pool_id_policy = policyForSignTxStakePoolRegistrationPoolId(
        txSigningMode,
        &certificate_data->poolId
    );
    LEDGER_ASSERT(pool_id_policy != POLICY_DENY, "Pool ID security policy denied");

    const pool_id_t *pool_id = &certificate_data->poolId;
    uint8_t pool_key_hash[POOL_KEY_HASH_LENGTH];

    switch (pool_id->keyReferenceType) {
        case KEY_REFERENCE_PATH:
            bip44_pathToKeyHash(&pool_id->path, pool_key_hash, sizeof(pool_key_hash));
            break;
        case KEY_REFERENCE_HASH:
            LEDGER_ASSERT(pool_id->hash != NULL, "NULL pool ID hash");
            memcpy(pool_key_hash, pool_id->hash, POOL_KEY_HASH_LENGTH);
            break;
        default:
            LEDGER_ASSERT(false, "Unsupported pool ID type");
    }

    if (pool_id_policy == POLICY_SHOW) {
        UI_ADD_FORMAT3(UI_STATIC_LABEL("Pool ID"),
                       MAX_BECH32_STRING_LENGTH,
                       format_bech32,
                       "pool",
                       pool_key_hash,
                       POOL_KEY_HASH_LENGTH);
    }

    security_policy_t vrf_policy = policyForSignTxStakePoolRegistrationVrfKey(txSigningMode);
    LEDGER_ASSERT(vrf_policy != POLICY_DENY, "VRF key security policy denied");

    if (vrf_policy == POLICY_SHOW) {
        UI_ADD_FORMAT3(UI_STATIC_LABEL("VRF key hash"),
                       MAX_BECH32_STRING_LENGTH,
                       format_bech32,
                       "vrf_vk",
                       certificate_data->vrfKeyHash,
                       VRF_KEY_HASH_LENGTH);
    }

    UI_ADD_FORMAT1(UI_STATIC_LABEL("Pledge"),
                   MAX_ADA_AMOUNT_STRING_LENGTH,
                   format_ada_amount,
                   certificate_data->poolRegistration.pledge);

    UI_ADD_FORMAT1(UI_STATIC_LABEL("Cost"),
                   MAX_ADA_AMOUNT_STRING_LENGTH,
                   format_ada_amount,
                   certificate_data->poolRegistration.cost);

    UI_ADD_FORMAT2(UI_STATIC_LABEL("Profit margin"),
                   MAX_PROFIT_MARGIN_STRING_LENGTH,
                   format_pool_margin,
                   certificate_data->poolRegistration.marginNumerator,
                   certificate_data->poolRegistration.marginDenominator);

    security_policy_t reward_policy = policyForSignTxStakePoolRegistrationRewardAccount(
        txSigningMode,
        G_context.tx_info.transaction.networkId,
        &certificate_data->poolRegistration.rewardAccount
    );
    LEDGER_ASSERT(reward_policy != POLICY_DENY, "Reward account security policy denied");

    if (reward_policy == POLICY_SHOW) {
        UI_ADD_FORMAT2(UI_STATIC_LABEL("Pool reward address"),
                       MAX_HUMAN_ADDRESS_LENGTH,
                       format_pool_reward_account,
                       G_context.tx_info.transaction.networkId,
                       &certificate_data->poolRegistration.rewardAccount);
    }

    uint32_t owner_index = 0;
    s_flist_node* owner_node = certificate_data->poolRegistration.poolOwners;
    while (owner_node != NULL) {
        tx_certificate_node_t* owner_item = (tx_certificate_node_t*) owner_node;
        ext_credential_t* owner_credential = &owner_item->certificate.stakeCredential;

        security_policy_t owner_policy = policyForSignTxStakePoolRegistrationOwner(
            G_context.tx_info.transaction.txSigningMode,
            owner_credential
        );
        LEDGER_ASSERT(owner_policy != POLICY_DENY, "Pool owner security policy denied");

        if (owner_policy == POLICY_SHOW) {
            UI_ADD_FORMAT2(UI_STATIC_LABEL("Owner reward address"),
                           MAX_HUMAN_ADDRESS_LENGTH,
                           format_reward_account_from_credential,
                           G_context.tx_info.transaction.networkId,
                           owner_credential);
        }

        owner_node = owner_node->next;
        owner_index++;
    }

    ASSERT(owner_index == pool_owner_counts.total_owners);
    if (pool_owner_counts.total_owners == 0) {
        warning_bits_set(&G_context.tx_info.warning_bits,
                         WARNING_BIT_POOL_REGISTRATION_NO_OWNERS);
        if (!ui_pairs_add_static_label_impl(UI_STATIC_LABEL("Pool owners"), (char *) UI_STATIC_LABEL("None"), false)) {
            ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
        }
    }

    uint32_t relay_index = 0;
    s_flist_node* relay_node = certificate_data->poolRegistration.relays;
    while (relay_node != NULL) {
        tx_certificate_node_t* relay_item = (tx_certificate_node_t*) relay_node;
        pool_relay_t* relay = (pool_relay_t*) &relay_item->certificate;

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
                UI_ADD_FORMAT1(UI_STATIC_LABEL("Relay"),
                               MAX_RELAY_INDEX_STRING_LENGTH,
                               format_index_with_prefix,
                               relay_index + 1);

                switch (relay->format) {
                    case RELAY_SINGLE_HOST_IP:
                        if (!relay->ipv4.isNull) {
                            UI_ADD_FORMAT1(UI_STATIC_LABEL("IPv4"), MAX_IPV4_STR_LENGTH, format_ipv4, &relay->ipv4);
                        }
                        if (!relay->ipv6.isNull) {
                            UI_ADD_FORMAT1(UI_STATIC_LABEL("IPv6"), MAX_IPV6_STR_LENGTH, format_ipv6, &relay->ipv6);
                        }
                        if (!relay->port.isNull) {
                            UI_ADD_FORMAT1(UI_STATIC_LABEL("Port"), MAX_UINT64_STRING_LENGTH, format_uint16, relay->port.number);
                        }
                        break;

                    case RELAY_SINGLE_HOST_NAME:
                        if (relay->dnsNameSize > 0) {
                            UI_ADD_FORMAT2(UI_STATIC_LABEL("DNS name"),
                                           MAX_DNS_NAME_LENGTH,
                                           format_dns_name,
                                           relay->dnsName,
                                           relay->dnsNameSize);
                        }
                        if (!relay->port.isNull) {
                            UI_ADD_FORMAT1(UI_STATIC_LABEL("Port"), MAX_UINT64_STRING_LENGTH, format_uint16, relay->port.number);
                        }
                        break;

                    case RELAY_MULTIPLE_HOST_NAME:
                        if (relay->dnsNameSize > 0) {
                            UI_ADD_FORMAT2(UI_STATIC_LABEL("SRV DNS"),
                                           MAX_DNS_NAME_LENGTH,
                                           format_dns_name,
                                           relay->dnsName,
                                           relay->dnsNameSize);
                        }
                        break;

                    default:
                        LEDGER_ASSERT(false, "Unknown relay type");
                }
                break;
            }
        }

        relay_node = relay_node->next;
        relay_index++;
    }

    ASSERT(relay_index == certificate_data->poolRegistration.numRelays);
    if (relay_index == 0) {
        warning_bits_set(&G_context.tx_info.warning_bits,
                         WARNING_BIT_POOL_REGISTRATION_NO_RELAYS);
        if (!ui_pairs_add_static_label_impl(UI_STATIC_LABEL("Pool relays"), (char *) UI_STATIC_LABEL("None"), false)) {
            ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
        }
    }

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
            UI_ADD_FORMAT2(UI_STATIC_LABEL("Pool metadata url"),
                           MAX_POOL_METADATA_URL_LENGTH,
                           format_url,
                           certificate_data->poolRegistration.poolMetadata.url,
                           certificate_data->poolRegistration.poolMetadata.urlSize);
            UI_ADD_FORMAT2(UI_STATIC_LABEL("Pool metadata hash"),
                           MAX_POOL_METADATA_HASH_STRING_LENGTH,
                           format_hex_bytes,
                           certificate_data->poolRegistration.poolMetadata.hash,
                           POOL_METADATA_HASH_LENGTH);
        }
    }
}

static bool should_show_pool_registration(
    const certificate_data_t *certificate_data,
    sign_tx_signingmode_t txSigningMode) {
    LEDGER_ASSERT(certificate_data != NULL, "NULL certificate data");

    security_policy_t generic_policy = policyForSignTxCertificate(
        txSigningMode,
        certificate_data->type
    );
    LEDGER_ASSERT(generic_policy != POLICY_DENY, "Certificate denied during UI");

    pool_owner_counts_t pool_owner_counts = count_pool_owner_nodes(
        certificate_data->poolRegistration.poolOwners
    );
    security_policy_t policy = policyForSignTxStakePoolRegistrationInit(
        txSigningMode,
        certificate_data->poolRegistration.numPoolOwners,
        pool_owner_counts.path_owners
    );
    LEDGER_ASSERT(policy != POLICY_DENY, "Certificate denied during UI");
    return policy == POLICY_SHOW;
}

static bool should_show_certificate(
    certificate_type_t certificate_type,
    const certificate_data_t *certificate_data,
    sign_tx_signingmode_t txSigningMode) {
    security_policy_t policy = POLICY_DENY;
    LEDGER_ASSERT(certificate_data != NULL, "NULL certificate data");

    security_policy_t generic_policy = policyForSignTxCertificate(
        txSigningMode,
        certificate_type
    );
    LEDGER_ASSERT(generic_policy != POLICY_DENY, "Certificate denied during UI");

    switch (certificate_type) {
        case CERTIFICATE_STAKE_REGISTRATION:
        case CERTIFICATE_STAKE_DEREGISTRATION:
        case CERTIFICATE_STAKE_REGISTRATION_CONWAY:
        case CERTIFICATE_STAKE_DEREGISTRATION_CONWAY:
        case CERTIFICATE_STAKE_DELEGATION:
            policy = policyForSignTxCertificateStaking(
                txSigningMode,
                certificate_type,
                &certificate_data->stakeCredential
            );
            break;

        case CERTIFICATE_VOTE_DELEGATION:
            policy = policyForSignTxCertificateVoteDelegation(
                txSigningMode,
                &certificate_data->stakeCredential,
                &certificate_data->drep
            );
            break;

        case CERTIFICATE_AUTHORIZE_COMMITTEE_HOT:
            policy = policyForSignTxCertificateCommitteeAuth(
                txSigningMode,
                &certificate_data->coldCredential,
                &certificate_data->hotCredential
            );
            break;

        case CERTIFICATE_RESIGN_COMMITTEE_COLD:
            policy = policyForSignTxCertificateCommitteeResign(
                txSigningMode,
                &certificate_data->coldCredential
            );
            break;

        case CERTIFICATE_DREP_REGISTRATION:
        case CERTIFICATE_DREP_DEREGISTRATION:
        case CERTIFICATE_DREP_UPDATE:
            policy = policyForSignTxCertificateDRep(
                txSigningMode,
                &certificate_data->dRepCredential
            );
            break;

        case CERTIFICATE_STAKE_POOL_RETIREMENT:
            policy = policyForSignTxCertificateStakePoolRetirement(
                txSigningMode,
                &certificate_data->poolCredential,
                certificate_data->retirementEpoch
            );
            break;

        case CERTIFICATE_STAKE_POOL_REGISTRATION:
            LEDGER_ASSERT(false, "CERTIFICATE_STAKE_POOL_REGISTRATION should be treated separately");
            return false;

        default:
            LEDGER_ASSERT(false, "Unknown certificate type");
            return false;
    }

    LEDGER_ASSERT(policy != POLICY_DENY, "Certificate denied during UI");
    return policy == POLICY_SHOW;
}

static void ui_strings_certificates(transaction_t *tx) {
    s_flist_node *node = tx->certificates;
    TRACE("Formatting %u certificates", tx->num_certificates);
    while (node != NULL) {
        tx_certificate_node_t *certificate_node = (tx_certificate_node_t *) node;
        if (certificate_node->certificate.type == CERTIFICATE_STAKE_POOL_REGISTRATION) {
            if (should_show_pool_registration(&certificate_node->certificate, tx->txSigningMode)) {
                TRACE("Formatting certificate type=%u", certificate_node->certificate.type);
                UI_ADD_FORMAT1(UI_STATIC_LABEL("Certificate"),
                               MAX_CERTIFICATE_TYPE_LENGTH,
                               format_certificate_type,
                               certificate_node->certificate.type);
                ui_strings_certificate_pool_registration(&certificate_node->certificate, tx->txSigningMode);
            }
        } else if (should_show_certificate(
                       certificate_node->certificate.type,
                       &certificate_node->certificate,
                       tx->txSigningMode)) {
            addCertificateUIPairs(&certificate_node->certificate);
        }

        node = node->next;
        app_mem_free(certificate_node);
    }
    tx->certificates = NULL;
}

static void ui_strings_withdrawals(transaction_t *tx) {
    s_flist_node *node = tx->withdrawals;
    TRACE("Formatting %u withdrawals", tx->num_withdrawals);
    while (node != NULL) {
        tx_withdrawal_node_t *withdrawal_node =
            (tx_withdrawal_node_t *) node;

        security_policy_t policy = policyForSignTxWithdrawal(
            tx->txSigningMode,
            &withdrawal_node->withdrawal.stakeCredential,
            &G_context.tx_info.warning_bits
        );

        switch (policy) {
            case POLICY_DENY:
                LEDGER_ASSERT(false, "Withdrawal denied during UI");
                break;
            case POLICY_SHOW:
                addWithdrawalUIPairs(
                    G_context.tx_info.transaction.networkId,
                    &withdrawal_node->withdrawal
                );
                break;
            case POLICY_HIDE:
                break;
        }

        node = node->next;
        app_mem_free(withdrawal_node);
    }
    tx->withdrawals = NULL;
}

static void ui_strings_aux_data_hash(transaction_t *tx) {
    if (tx->includeAuxDataHash) {
        security_policy_t policy = policyForSignTxAuxData(tx->auxDataType);
        LEDGER_ASSERT(policy != POLICY_DENY, "Aux data denied during UI");
        if (policy == POLICY_SHOW) {
            UI_ADD_FORMAT2(UI_STATIC_LABEL("Auxiliary data hash"), MAX_TX_HASH_DISPLAY_LENGTH, format_hex_bytes, tx->auxDataHash, AUX_DATA_HASH_LENGTH);
        }
    }
}

static void ui_strings_validity_interval_start(transaction_t *tx) {
    if (!tx->includeValidityIntervalStart) {
        return;
    }
    security_policy_t validity_interval_start_policy = policyForSignTxValidityIntervalStart();
    switch (validity_interval_start_policy) {
        case POLICY_DENY:
            LEDGER_ASSERT(false, "Validity interval start denied during UI");
            break;
        case POLICY_SHOW:
            UI_ADD_FORMAT3(UI_STATIC_LABEL("Validity interval start"), MAX_VALIDITY_BOUNDARY_STRING_LENGTH, format_validity_boundary, tx->validityIntervalStart, tx->networkId, tx->protocolMagic);
            break;
        case POLICY_HIDE:
            break;
    }
}

// Local formatter for mint summary display (e.g., "2 asset groups", "1 asset group")
static bool format_mint_summary(uint16_t num_groups, char *out, size_t outSize) {
    snprintf(out, outSize, "%u asset group%s", num_groups, (num_groups == 1) ? "" : "s");
    size_t len = strlen(out);
    return len < outSize;
}

static void ui_strings_mint(transaction_t *tx) {
    if (tx->mint_asset_groups == NULL) {
        return;
    }

    security_policy_t mint_policy = policyForSignTxMintInit(tx->txSigningMode);
    LEDGER_ASSERT(mint_policy != POLICY_DENY, "Mint denied during UI");
    const bool show_mint = (mint_policy == POLICY_SHOW);

    if (show_mint) {
        UI_ADD_FORMAT1(UI_STATIC_LABEL("Mint"), MAX_MINT_SUMMARY_STRING_LENGTH, format_mint_summary, tx->num_mint_asset_groups);
    }

    s_flist_node *node = tx->mint_asset_groups;
    while (node != NULL) {
        mint_asset_group_node_t *asset_group_node =
            (mint_asset_group_node_t *) node;
        node = node->next;

        ASSERT(asset_group_node->asset_group.policyId != NULL);
        s_flist_node *token_node = asset_group_node->asset_group.tokens;
        while (token_node != NULL) {
            mint_token_node_t *token_node_entry = (mint_token_node_t *) token_node;
            mint_token_t *token = &token_node_entry->token;
            token_node = token_node->next;

            if (show_mint) {
                UI_ADD_FORMAT3(UI_STATIC_LABEL("Mint fingerprint"), MAX_TOKEN_FINGERPRINT_STRING_LENGTH, format_asset_fingerprint_bech32, asset_group_node->asset_group.policyId, token->assetName, token->assetNameLen);
                UI_ADD_FORMAT4(UI_STATIC_LABEL("Mint amount"), MAX_MINT_AMOUNT_STRING_LENGTH, format_token_amount_mint, asset_group_node->asset_group.policyId, token->assetName, token->assetNameLen, token->amount);
            }

            app_mem_free(token_node_entry);
        }

        asset_group_node->asset_group.tokens = NULL;
        app_mem_free(asset_group_node);
    }

    tx->mint_asset_groups = NULL;
}

static void ui_strings_script_data_hash(transaction_t *tx) {
    if (tx->includeScriptDataHash) {
        security_policy_t policy = policyForSignTxScriptDataHash(tx->txSigningMode);
        LEDGER_ASSERT(policy != POLICY_DENY, "Script data hash denied during UI");
        if (policy == POLICY_SHOW) {
            UI_ADD_FORMAT2(UI_STATIC_LABEL("Script data hash"), MAX_TX_HASH_DISPLAY_LENGTH, format_hex_bytes, tx->scriptDataHash, SCRIPT_DATA_HASH_LENGTH);
        }
    }
}

static void ui_strings_collateral_inputs(transaction_t *tx) {
    s_flist_node *node = tx->collateral_inputs;
    while (node != NULL) {
        tx_collateral_input_node_t *collateral_input_node = (tx_collateral_input_node_t *) node;

        security_policy_t collateral_input_policy = policyForSignTxCollateralInput(
            tx->txSigningMode,
            tx->includeTotalCollateral,
            &collateral_input_node->input);
        LEDGER_ASSERT(collateral_input_policy != POLICY_DENY, "Collateral input policy denied during UI");

        if (collateral_input_policy == POLICY_SHOW) {
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Coll input"), MAX_INPUT_DISPLAY_STRING_LENGTH, format_input_with_index, &collateral_input_node->input);
        }

        node = node->next;
        app_mem_free(collateral_input_node);
    }
    tx->collateral_inputs = NULL;
}

static void ui_strings_required_signers(transaction_t *tx) {
    s_flist_node *node = tx->required_signers;
    while (node != NULL) {
        tx_required_signer_node_t *required_signer_node = (tx_required_signer_node_t *) node;
        required_signer_t *required_signer = &required_signer_node->required_signer;

        security_policy_t policy = policyForSignTxRequiredSigner(tx->txSigningMode, required_signer);
        LEDGER_ASSERT(policy != POLICY_DENY, "Required signer denied during UI");

        if (policy == POLICY_SHOW) {
            switch (required_signer->type) {
                case REQUIRED_SIGNER_WITH_HASH: {
                    UI_ADD_FORMAT3(UI_STATIC_LABEL("Required signer"), MAX_BECH32_STRING_LENGTH, format_bech32, "vkh", required_signer->keyHash, ADDRESS_KEY_HASH_LENGTH);
                    break;
                }
                case REQUIRED_SIGNER_WITH_PATH: {
                    UI_ADD_FORMAT1(UI_STATIC_LABEL("Required signer"), MAX_BIP44_PATH_STRING_LENGTH, format_bip44_path, &required_signer->keyPath);
                    break;
                }
                default:
                    LEDGER_ASSERT(false, "Unknown required signer type");
            }
        }

        node = node->next;
        app_mem_free(required_signer_node); // only after next is assigned
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
        .includeRefScript = tx->collateral_output.refScript.hasRefScript,
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
            addPaymentInfoUIPairs(collateral_desc.destination.params);
            addStakingInfoUIPairs(collateral_desc.destination.params);
        }

        if (collateral_ada_policy == POLICY_SHOW) {
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Collateral amount"), MAX_ADA_AMOUNT_STRING_LENGTH, format_ada_amount, collateral_desc.amount);
        }

        if (collateral_confirm_policy == POLICY_SHOW) {
            warning_bits_set(&G_context.tx_info.warning_bits, WARNING_BIT_COLLATERAL_OUTPUT_WARNING);
        }
    }

    ui_format_and_free_output_asset_groups(
        tx->collateral_output.assetGroups,
        tx->collateral_output.numAssetGroups,
        show_collateral_tokens);

    tx->collateral_output.assetGroups = NULL;
}

static void ui_strings_total_collateral(transaction_t *tx) {
    if (!tx->includeTotalCollateral) {
        return;
    }
    security_policy_t policy = policyForSignTxTotalCollateral();
    LEDGER_ASSERT(policy != POLICY_DENY, "Total collateral denied during UI");
    if (policy == POLICY_SHOW) {
        UI_ADD_FORMAT1(UI_STATIC_LABEL("Total collateral"), MAX_ADA_AMOUNT_STRING_LENGTH, format_ada_amount, tx->totalCollateral);
    }
}

static void ui_strings_reference_inputs(transaction_t *tx) {
    s_flist_node *ref_input_node = tx->reference_inputs;
    while (ref_input_node != NULL) {
        tx_input_node_t *ref_input = (tx_input_node_t *) ref_input_node;

        security_policy_t reference_input_policy = policyForSignTxReferenceInput(
            tx->txSigningMode,
            &ref_input->input);
        LEDGER_ASSERT(reference_input_policy != POLICY_DENY, "Reference input denied during UI");

        if (reference_input_policy == POLICY_SHOW) {
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Ref input"), MAX_INPUT_DISPLAY_STRING_LENGTH, format_input_with_index, &ref_input->input);
        }

        ref_input_node = ref_input_node->next;
        app_mem_free(ref_input);
    }
    tx->reference_inputs = NULL;
}

static void ui_strings_voting_procedures(transaction_t *tx) {
    if (tx->num_voters > 0) {
        s_flist_node *node = tx->voting_procedures;
        while (node != NULL) {
            voter_votes_list_item_t *voter_node = (voter_votes_list_item_t *) node;

            security_policy_t policy = policyForSignTxVotingProcedure(tx->txSigningMode, &voter_node->voter_votes_data.voter);
            LEDGER_ASSERT(policy != POLICY_DENY, "Voting procedure denied during UI");

            if (policy == POLICY_SHOW) {
                // Display Voter
                addVoterUIPairs(&voter_node->voter_votes_data.voter);
                // Iterate votes
                s_flist_node *vote_node = voter_node->voter_votes_data.votes;
                while (vote_node != NULL) {
                    vote_list_item_t *vote_node_data = (vote_list_item_t *) vote_node;

                    // Gov Action Tx Hash
                    UI_ADD_FORMAT2(UI_STATIC_LABEL("Gov action tx hash"), MAX_TX_HASH_DISPLAY_LENGTH, format_hex_bytes, vote_node_data->vote_data.govActionId.txHash, TX_HASH_LENGTH);

                    // Gov Action Index
                    UI_ADD_FORMAT1(UI_STATIC_LABEL("Gov action index"), MAX_UINT64_STRING_LENGTH, format_uint64, vote_node_data->vote_data.govActionId.govActionIndex);

                    // Vote Option
                    UI_ADD_FORMAT1(UI_STATIC_LABEL("Vote"), MAX_VOTE_OPTION_LENGTH, format_vote_option, vote_node_data->vote_data.voteOption);

                    // Anchor
                    addAnchorUIPairs(&vote_node_data->vote_data.anchor);

                    vote_node = vote_node->next;
                }
            }

            s_flist_node *vote_node = voter_node->voter_votes_data.votes;
            while (vote_node != NULL) {
                s_flist_node *vote_node_to_free = vote_node;
                vote_node = vote_node->next;
                app_mem_free(vote_node_to_free);
            }
            voter_node->voter_votes_data.votes = NULL;
            node = node->next;
            app_mem_free(voter_node);
        }
    }
    tx->voting_procedures = NULL;
}

static void ui_strings_treasury(transaction_t *tx) {
    if (tx->includeTreasury) {
        security_policy_t policy = policyForSignTxTreasury(tx->txSigningMode, tx->treasury);
        LEDGER_ASSERT(policy != POLICY_DENY, "Treasury denied during UI");
        if (policy == POLICY_SHOW) {
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Treasury"), MAX_ADA_AMOUNT_STRING_LENGTH, format_ada_amount, tx->treasury);
        }
    }
}

static void ui_strings_donation(transaction_t *tx) {
    if (tx->includeDonation) {
        security_policy_t policy = policyForSignTxDonation(tx->txSigningMode, tx->donation);
        LEDGER_ASSERT(policy != POLICY_DENY, "Donation denied during UI");
        if (policy == POLICY_SHOW) {
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Donation"), MAX_ADA_AMOUNT_STRING_LENGTH, format_ada_amount, tx->donation);
        }
    }
}

static void ui_strings_tx_hash(void) {
    if (G_context.tx_info.raw_tx != NULL) {
        app_mem_free(G_context.tx_info.raw_tx);
        G_context.tx_info.raw_tx = NULL;
        G_context.tx_info.raw_tx_len = 0;
    }

    UI_ADD_FORMAT2(UI_STATIC_LABEL("Transaction hash"), MAX_TX_HASH_DISPLAY_LENGTH, format_hex_bytes, G_context.tx_info.tx_hash, sizeof(G_context.tx_info.tx_hash));
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
