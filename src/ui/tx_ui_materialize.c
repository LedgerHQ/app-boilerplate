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
#include "ui/display.h"
#include "ui/ui_utils.h"
#include "ui/tx_ui_helpers.h"
#include "io.h"
#include "utils/cardano_os_utils.h"
#include "app_tokens/app_tokens.h"
#include "transaction/tx_voting_procedure_types.h"
#include "addressUtils/bech32.h"

// Max display lengths
#define MAX_DATUM_HASH_STRING_LENGTH (2 * OUTPUT_DATUM_HASH_LENGTH + 1)
#define MAX_POOL_METADATA_HASH_STRING_LENGTH (2 * POOL_METADATA_HASH_LENGTH + 1)
#define MAX_INPUT_DISPLAY_STRING_LENGTH (MAX_TX_HASH_DISPLAY_LENGTH + 3 + MAX_UINT64_STRING_LENGTH)
static nbgl_warning_t *g_warning = NULL;

static char *ui_alloc_temp(size_t size) {
    if (size == 0) {
        return NULL;
    }
    char *tmp = (char *) app_mem_alloc(size);
    if (tmp != NULL) {
        explicit_bzero(tmp, size);
    }
    return tmp;
}


static int format_input_with_index(char *out, size_t out_size, const tx_input_t *input) {
    LEDGER_ASSERT(out != NULL, "NULL output buffer");
    LEDGER_ASSERT(input != NULL, "NULL input");
    int hex_status = bytes_to_lowercase_hex(out, out_size, input->txHash, TX_HASH_LENGTH);
    LEDGER_ASSERT(hex_status == 0, "Input hash hex formatting failed");
    size_t hash_len = strlen(out);
    if (hash_len + 1 >= out_size) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    snprintf(out + hash_len, out_size - hash_len, " / %u", input->index);
    LEDGER_ASSERT(strlen(out) < out_size, "Input display buffer overflow");
    return SWO_SUCCESS;
}

static int ui_materialize_token_groups(asset_group_t* assetGroups,
                                       uint16_t numGroups,
                                       bool show_tokens) {
    if (assetGroups == NULL) {
        return SWO_SUCCESS;
    }

    for (uint16_t ag = 0; ag < numGroups; ag++) {
        asset_group_t *group = &assetGroups[ag];
        s_flist_node *token_node = group->tokens;
        while (token_node != NULL) {
            output_token_list_item_t *token_item = (output_token_list_item_t *) token_node;
            output_token_t *token = &token_item->token_data;
            s_flist_node *token_next = token_node->next;

            if (show_tokens) {
                char *fingerprint_tmp = ui_alloc_temp(MAX_TOKEN_FINGERPRINT_STRING_LENGTH + 1);
                if (fingerprint_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                size_t fingerprint_len = deriveAssetFingerprintBech32(
                    group->policyId,
                    MINTING_POLICY_ID_LENGTH,
                    token->assetName,
                    token->assetNameLen,
                    fingerprint_tmp,
                    MAX_TOKEN_FINGERPRINT_STRING_LENGTH + 1);
                LEDGER_ASSERT(fingerprint_len > 0, "Fingerprint derivation failed");
                int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Asset fingerprint"), fingerprint_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                if (status != SWO_SUCCESS) {
                    return status;
                }
                char *token_amount_tmp = ui_alloc_temp(MAX_TOKEN_AMOUNT_OUTPUT_STRING_LENGTH + 1);
                if (token_amount_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                token_group_t tokenGroup;
                memcpy(tokenGroup.policyId, group->policyId, MINTING_POLICY_ID_LENGTH);
                bool token_amount_formatted = str_formatTokenAmountOutput(
                    &tokenGroup,
                    token->assetName,
                    token->assetNameLen,
                    token->amount,
                    token_amount_tmp,
                    MAX_TOKEN_AMOUNT_OUTPUT_STRING_LENGTH + 1);
                ASSERT(token_amount_formatted);
                status = ui_pairs_add_static_label(UI_STATIC_LABEL("Token amount"), token_amount_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                if (status != SWO_SUCCESS) {
                    return status;
                }
            }

            app_mem_free(token_node);
            token_node = token_next;
        }
        group->tokens = NULL;
    }

    return SWO_SUCCESS;
}

static int ui_strings_inputs(transaction_t *tx) {
    security_policy_t input_policy = policyForSignTxInput(tx->txSigningMode);
    LEDGER_ASSERT(input_policy != POLICY_DENY, "Input denied during UI");
    s_flist_node *input_node = tx->inputs;
    while (input_node != NULL) {
        tx_input_list_item_t *input_item = (tx_input_list_item_t *) input_node;
        s_flist_node *next = input_node->next;

        if (input_policy == POLICY_SHOW) {
            char *input_tmp = ui_alloc_temp(MAX_INPUT_DISPLAY_STRING_LENGTH + 1);
            if (input_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;
            int status = format_input_with_index(input_tmp, MAX_INPUT_DISPLAY_STRING_LENGTH + 1, &input_item->input_data);
            if (status != SWO_SUCCESS) return status;
            status = ui_pairs_add_static_label(UI_STATIC_LABEL("Input"), input_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
            if (status != SWO_SUCCESS) return status;
        }

        app_mem_free(input_item);
        input_node = next;
    }
    tx->inputs = NULL;
    return SWO_SUCCESS;
}

static int ui_strings_outputs(transaction_t *tx) {
    uint16_t output_num = 1;
    s_flist_node *output_node = tx->outputs;
    TRACE("Materializing %u outputs", tx->num_outputs);
    while (output_node != NULL) {
        tx_output_list_item_t *output_item = (tx_output_list_item_t *) output_node;
        s_flist_node *next = output_node->next;

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
                char *output_num_tmp = ui_alloc_temp(MAX_UINT64_STRING_LENGTH + 2);
                if (output_num_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                snprintf(output_num_tmp, MAX_UINT64_STRING_LENGTH + 2, "#%d", output_num);
                LEDGER_ASSERT(strlen(output_num_tmp) <= MAX_UINT64_STRING_LENGTH, "Output number ui string buffer too short");
                int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Output"), output_num_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                if (status != SWO_SUCCESS) {
                    return status;
                }
                char *address_tmp = ui_alloc_temp(MAX_HUMAN_ADDRESS_LENGTH + 1);
                if (address_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }

                bool address_formatted = false;
                if (output_item->output_data.destination.type == DESTINATION_THIRD_PARTY) {
                    address_formatted = format_address_human_readable(
                        output_item->output_data.destination.address.buffer,
                        output_item->output_data.destination.address.size,
                        address_tmp,
                        MAX_HUMAN_ADDRESS_LENGTH + 1
                    );
                } else {
                    uint8_t address_bytes[MAX_ADDRESS_LENGTH];
                    size_t derived_len = deriveAddress(
                        &output_item->output_data.destination.params,
                        address_bytes,
                        sizeof(address_bytes)
                    );
                    if (derived_len > 0) {
                        address_formatted = format_address_human_readable(
                            address_bytes,
                            derived_len,
                            address_tmp,
                            MAX_HUMAN_ADDRESS_LENGTH + 1
                        );
                    }
                }

                LEDGER_ASSERT(address_formatted, "Address formatting failed");
                size_t address_len = strlen(address_tmp);
                LEDGER_ASSERT(address_len > 0, "Address length zero");
                LEDGER_ASSERT(address_len < MAX_HUMAN_ADDRESS_LENGTH + 1, "Address truncated");

                status = ui_pairs_add_static_label(UI_STATIC_LABEL("Address"), address_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                if (status != SWO_SUCCESS) {
                    return status;
                }
                char *amount_tmp = ui_alloc_temp(MAX_ADA_AMOUNT_STRING_LENGTH + 1);
                if (amount_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                bool amount_formatted = str_formatAdaAmount(output_item->output_data.adaAmount,
                                                            amount_tmp,
                                                            MAX_ADA_AMOUNT_STRING_LENGTH + 1);
                ASSERT(amount_formatted);
                status = ui_pairs_add_static_label(UI_STATIC_LABEL("Amount"), amount_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                if (status != SWO_SUCCESS) {
                    return status;
                }
                if (output_item->output_data.datum.hasDatum && datum_policy == POLICY_SHOW) {
                    // TODO: Inline datum size is not bounded by protocol; handle large values more robustly.
                    if (output_item->output_data.datum.type == DATUM_HASH) {
                        char *datum_value_tmp = ui_alloc_temp(MAX_DATUM_HASH_STRING_LENGTH + 2);
                        if (datum_value_tmp == NULL) {
                            return SWO_INSUFFICIENT_MEMORY;
                        }
                        int hex_status = bytes_to_lowercase_hex(
                            datum_value_tmp,
                            MAX_DATUM_HASH_STRING_LENGTH + 2,
                            output_item->output_data.datum.hash,
                            OUTPUT_DATUM_HASH_LENGTH);
                        LEDGER_ASSERT(hex_status == 0, "Datum hash formatting failed");
                        LEDGER_ASSERT(strlen(datum_value_tmp) <= MAX_DATUM_HASH_STRING_LENGTH, "Datum ui string buffer too short");
                        status = ui_pairs_add_static_label(UI_STATIC_LABEL("Datum hash"), datum_value_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                    } else {
                        // TODO not enough space for inline datum, how big a buffer to use here?
                        // TODO these unlimited items in UI should perhaps be detected upfront, we can go over the whole tx and check if some individual field
                        // TODO is too big for UI and run it via some streaming UI
                        const int max_len = 100;
                        char *datum_value_tmp = ui_alloc_temp(max_len + 2);
                        if (datum_value_tmp == NULL) {
                            return SWO_INSUFFICIENT_MEMORY;
                        }
                        uint16_t inline_size = output_item->output_data.datum.inline_data.size;
                        snprintf(datum_value_tmp, max_len + 2, "Inline datum (%u bytes)", inline_size);
                        LEDGER_ASSERT(strlen(datum_value_tmp) <= MAX_DATUM_HASH_STRING_LENGTH, "Datum ui string buffer too short");

                        status = ui_pairs_add_static_label(UI_STATIC_LABEL("Inline datum"), datum_value_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                    }
                }

                if (output_item->output_data.hasRefScript && ref_script_policy == POLICY_SHOW) {
                    // TODO: Reference script size is not bounded by protocol; handle large values more robustly.
                    char *refscript_tmp = ui_alloc_temp(MAX_REFERENCE_SCRIPT_STRING_LENGTH + 2);
                    if (refscript_tmp == NULL) {
                        return SWO_INSUFFICIENT_MEMORY;
                    }
                    snprintf(refscript_tmp,
                             MAX_REFERENCE_SCRIPT_STRING_LENGTH + 2,
                             "Reference script (%u bytes)",
                             output_item->output_data.refScript.size);
                    LEDGER_ASSERT(strlen(refscript_tmp) <= MAX_REFERENCE_SCRIPT_STRING_LENGTH, "Reference script ui string buffer too short");
                    status = ui_pairs_add_static_label(UI_STATIC_LABEL("Reference script"), refscript_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                    if (status != SWO_SUCCESS) {
                        return status;
                    }
                }

                if (output_item->output_data.assetGroups != NULL) {
                    status = ui_materialize_token_groups(
                        output_item->output_data.assetGroups,
                        output_item->output_data.numAssetGroups,
                        true
                    );
                    if (status != SWO_SUCCESS) {
                        return status;
                    }
                }

                output_num++;
            }
            break;
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
        output_node = next;
    }
    tx->outputs = NULL;

    return SWO_SUCCESS;
}

static int ui_strings_fee(transaction_t *tx) {
    char *fee_tmp = ui_alloc_temp(MAX_ADA_AMOUNT_STRING_LENGTH + 1);
    if (fee_tmp == NULL) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    bool fee_formatted = str_formatAdaAmount(tx->fee, fee_tmp, MAX_ADA_AMOUNT_STRING_LENGTH + 1);
    ASSERT(fee_formatted);
    int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Fee"), fee_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
    if (status != SWO_SUCCESS) {
        return status;
    }
    return SWO_SUCCESS;
}

static int ui_strings_ttl(transaction_t *tx) {
    if (!tx->includeTtl) {
        return SWO_SUCCESS;
    }
    security_policy_t ttl_policy = policyForSignTxTtl(tx->ttl);
    LEDGER_ASSERT(ttl_policy != POLICY_DENY, "TTL denied during UI");
    switch (ttl_policy) {
        case POLICY_DENY:
            // Already asserted above, this case should never be reached
            break;
        case POLICY_SHOW: {
            char *ttl_tmp = ui_alloc_temp(MAX_VALIDITY_BOUNDARY_STRING_LENGTH + 1);
            if (ttl_tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            bool ttl_formatted = str_formatValidityBoundary(tx->ttl,
                                                            tx->networkId,
                                                            tx->protocolMagic,
                                                            ttl_tmp,
                                                            MAX_VALIDITY_BOUNDARY_STRING_LENGTH + 1);
            ASSERT(ttl_formatted);
            int status = ui_pairs_add_static_label(UI_STATIC_LABEL("TTL"), ttl_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
            if (status != SWO_SUCCESS) {
                return status;
            }
            break;
        }
        case POLICY_HIDE:
            break;
    }
    return SWO_SUCCESS;
}

// Helper functions for common credential display patterns

static int display_stake_credential(const ext_credential_t* credential) {
    return addCredentialUIPairs(
        credential,
        UI_STATIC_LABEL("Stake key"),
        UI_STATIC_LABEL("Stake key hash"),
        "stake_vkh",
        UI_STATIC_LABEL("Stake script hash"),
        "script"
    );
}

static int display_drep_credential(const ext_credential_t* credential) {
    return addCredentialUIPairs(
        credential,
        UI_STATIC_LABEL("DRep key"),
        UI_STATIC_LABEL("DRep key hash"),
        "drep",
        UI_STATIC_LABEL("DRep script hash"),
        "drep"
    );
}

static int display_committee_cold_credential(const ext_credential_t* credential) {
    return addCredentialUIPairs(
        credential,
        UI_STATIC_LABEL("Committee cold key"),
        UI_STATIC_LABEL("Committee cold key hash"),
        "cc_cold",
        UI_STATIC_LABEL("Committee cold script hash"),
        "cc_cold"
    );
}

static int display_committee_hot_credential(const ext_credential_t* credential) {
    return addCredentialUIPairs(
        credential,
        UI_STATIC_LABEL("Committee hot key"),
        UI_STATIC_LABEL("Committee hot key hash"),
        "cc_hot",
        UI_STATIC_LABEL("Committee hot script hash"),
        "cc_hot_script"
    );
}

static int display_voter_credential(const ext_credential_t* credential) {
    return addCredentialUIPairs(
        credential,
        UI_STATIC_LABEL("Voter"),
        UI_STATIC_LABEL("Voter hash"),
        "stake_vkh",
        UI_STATIC_LABEL("Voter script hash"),
        "script"
    );
}

static int ui_strings_certificate_stake_registration(const certificate_data_t* certificate_data) {
    return display_stake_credential(&certificate_data->stakeCredential);
}

static int ui_strings_certificate_stake_delegation(const certificate_data_t* certificate_data) {
    int status;

    status = display_stake_credential(&certificate_data->stakeCredential);
    if (status != SWO_SUCCESS) {
        return status;
    }

    status = addPoolKeyHashUIPairs(certificate_data->poolKeyHash, UI_STATIC_LABEL("Pool"));
    if (status != SWO_SUCCESS) {
        return status;
    }
    return SWO_SUCCESS;
}

static int ui_strings_certificate_stake_conway(const certificate_data_t* certificate_data) {
    int status;
    // Display stake credential
    status = display_stake_credential(&certificate_data->stakeCredential);
    if (status != SWO_SUCCESS) {
        return status;
    }

    // Display deposit
    status = addDepositUIPairs(certificate_data->deposit, UI_STATIC_LABEL("Deposit"));
    if (status != SWO_SUCCESS) {
        return status;
    }
    return SWO_SUCCESS;
}

static int ui_strings_certificate_pool_retirement(const certificate_data_t* certificate_data) {
    int status;
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
    status = addPoolKeyHashUIPairs(poolKeyHash, UI_STATIC_LABEL("Pool ID"));
    if (status != SWO_SUCCESS) {
        return status;
    }

    // Display retirement epoch
    char *epoch_tmp = ui_alloc_temp(MAX_UINT64_STRING_LENGTH + 1);
    if (epoch_tmp == NULL) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    bool epoch_formatted = format_u64(epoch_tmp, MAX_UINT64_STRING_LENGTH + 1,
                                        certificate_data->retirementEpoch);
    LEDGER_ASSERT(epoch_formatted, "Failed to format retirement epoch");
    status = ui_pairs_add_static_label(UI_STATIC_LABEL("Retirement epoch"), epoch_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
    if (status != SWO_SUCCESS) {
        return status;
    }
    return SWO_SUCCESS;
}

static int ui_strings_certificate_vote_delegation(const certificate_data_t* certificate_data) {
    int status;

    status = display_voter_credential(&certificate_data->stakeCredential);
    if (status != SWO_SUCCESS) {
        return status;
    }

    const ext_drep_t* drep = &certificate_data->drep;
    status = addDRepUIPairs(drep, UI_STATIC_LABEL("DRep"));
    if (status != SWO_SUCCESS) {
        return status;
    }
    return SWO_SUCCESS;
}

static int ui_strings_certificate_committee_hot(const certificate_data_t* certificate_data) {
    int status;

    const ext_credential_t* coldCred = &certificate_data->coldCredential;
    status = display_committee_cold_credential(coldCred);
    if (status != SWO_SUCCESS) {
        return status;
    }

    const ext_credential_t* hotCred = &certificate_data->hotCredential;
    status = display_committee_hot_credential(hotCred);
    if (status != SWO_SUCCESS) {
        return status;
    }
    return SWO_SUCCESS;
}

static int ui_strings_certificate_committee_resign(const certificate_data_t* certificate_data) {
    int status;

    status = display_committee_cold_credential(&certificate_data->coldCredential);
    if (status != SWO_SUCCESS) {
        return status;
    }

    status = addAnchorUIPairs(&certificate_data->anchor);
    if (status != SWO_SUCCESS) {
        return status;
    }
    return SWO_SUCCESS;
}

static int ui_strings_certificate_drep_registration(const certificate_data_t* certificate_data) {
    int status;

    status = display_drep_credential(&certificate_data->dRepCredential);
    if (status != SWO_SUCCESS) {
        return status;
    }

    status = addDepositUIPairs(certificate_data->deposit, UI_STATIC_LABEL("Deposit"));
    if (status != SWO_SUCCESS) {
        return status;
    }

    status = addAnchorUIPairs(&certificate_data->anchor);
    if (status != SWO_SUCCESS) {
        return status;
    }
    return SWO_SUCCESS;
}

static int ui_strings_certificate_drep_deregistration(const certificate_data_t* certificate_data) {
    int status;

    status = display_drep_credential(&certificate_data->dRepCredential);
    if (status != SWO_SUCCESS) {
        return status;
    }

    status = addDepositUIPairs(certificate_data->deposit, UI_STATIC_LABEL("Deposit"));
    if (status != SWO_SUCCESS) {
        return status;
    }
    return SWO_SUCCESS;
}

static int ui_strings_certificate_drep_update(const certificate_data_t* certificate_data) {
    int status;

    status = display_drep_credential(&certificate_data->dRepCredential);
    if (status != SWO_SUCCESS) {
        return status;
    }

    status = addAnchorUIPairs(&certificate_data->anchor);
    if (status != SWO_SUCCESS) {
        return status;
    }
    return SWO_SUCCESS;
}

static int ui_strings_certificate_pool_registration(const certificate_data_t* certificate_data, sign_tx_signingmode_t txSigningMode) {
    int status;
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
        status = addPoolKeyHashUIPairs(poolKeyHash, UI_STATIC_LABEL("Pool ID"));
        if (status != SWO_SUCCESS) {
            return status;
        }
    }

    // Check VRF key security policy
    security_policy_t vrf_policy = policyForSignTxStakePoolRegistrationVrfKey(txSigningMode);
    LEDGER_ASSERT(vrf_policy != POLICY_DENY, "VRF key security policy denied");

    // Display VRF key hash
    if (vrf_policy == POLICY_SHOW) {
        char *vrf_hash_tmp = ui_alloc_temp(MAX_BECH32_STRING_LENGTH + 1);
        if (vrf_hash_tmp == NULL) {
            return SWO_INSUFFICIENT_MEMORY;
        }
        bool encoded = format_bech32("vrf_vk",
                            certificate_data->vrfKeyHash,
                            VRF_KEY_HASH_LENGTH,
                            vrf_hash_tmp,
                            MAX_BECH32_STRING_LENGTH + 1);
        LEDGER_ASSERT(encoded, "Unable to format VRF key hash");
        status = ui_pairs_add_static_label(UI_STATIC_LABEL("VRF key hash"), vrf_hash_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
        if (status != SWO_SUCCESS) {
            return status;
        }
    }

    // Display pledge
    char *pledge_tmp = ui_alloc_temp(MAX_ADA_AMOUNT_STRING_LENGTH + 1);
    if (pledge_tmp == NULL) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    bool pledge_formatted = str_formatAdaAmount(
        certificate_data->poolRegistration.pledge,
        pledge_tmp,
        MAX_ADA_AMOUNT_STRING_LENGTH + 1);
    ASSERT(pledge_formatted);
    status = ui_pairs_add_static_label(UI_STATIC_LABEL("Pledge"), pledge_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
    if (status != SWO_SUCCESS) {
        return status;
    }

    // Display cost
    char *cost_tmp = ui_alloc_temp(MAX_ADA_AMOUNT_STRING_LENGTH + 1);
    if (cost_tmp == NULL) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    bool cost_formatted = str_formatAdaAmount(
        certificate_data->poolRegistration.cost,
        cost_tmp,
        MAX_ADA_AMOUNT_STRING_LENGTH + 1);
    ASSERT(cost_formatted);
    status = ui_pairs_add_static_label(UI_STATIC_LABEL("Cost"), cost_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
    if (status != SWO_SUCCESS) {
        return status;
    }

    // Display profit margin as percentage
    // Similar to old app: convert to percentage (0-10000 basis points)
    char *margin_tmp = ui_alloc_temp(MAX_PROFIT_MARGIN_STRING_LENGTH + 2);
    if (margin_tmp == NULL) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    uint64_t margin_num = certificate_data->poolRegistration.marginNumerator;
    uint64_t margin_den = certificate_data->poolRegistration.marginDenominator;
    uint64_t margin_percentage = (10000 * margin_num + (margin_den / 2)) / margin_den;
    const unsigned int percentage = (unsigned int) margin_percentage;
    snprintf(margin_tmp, MAX_PROFIT_MARGIN_STRING_LENGTH + 2, "%u.%u %%", percentage / 100, percentage % 100);
    LEDGER_ASSERT(strlen(margin_tmp) <= MAX_PROFIT_MARGIN_STRING_LENGTH, "Profit margin ui string buffer too short");
    status = ui_pairs_add_static_label(UI_STATIC_LABEL("Profit margin"), margin_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
    if (status != SWO_SUCCESS) {
        return status;
    }

    // Check reward account security policy
    security_policy_t reward_policy = policyForSignTxStakePoolRegistrationRewardAccount(
        txSigningMode,
        &certificate_data->poolRegistration.rewardAccount
    );
    LEDGER_ASSERT(reward_policy != POLICY_DENY, "Reward account security policy denied");

    // Display reward account
    if (reward_policy == POLICY_SHOW) {
        char *reward_account = NULL;
        switch (certificate_data->poolRegistration.rewardAccount.keyReferenceType) {
            case KEY_REFERENCE_PATH:
                reward_account = ui_alloc_temp(strlen("key path") + 1);
                if (reward_account == NULL) return SWO_INSUFFICIENT_MEMORY;
                memcpy(reward_account, "key path", strlen("key path") + 1);
                status = ui_pairs_add_static_label(UI_STATIC_LABEL("Reward account"), reward_account) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                break;
            case KEY_REFERENCE_HASH:
                reward_account = ui_alloc_temp(strlen("key hash") + 1);
                if (reward_account == NULL) return SWO_INSUFFICIENT_MEMORY;
                memcpy(reward_account, "key hash", strlen("key hash") + 1);
                status = ui_pairs_add_static_label(UI_STATIC_LABEL("Reward account"), reward_account) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                break;
            default:
                LEDGER_ASSERT(false, "Invalid reward account type");
        }
        if (status != SWO_SUCCESS) {
            return status;
        }
    }

    // Display pool owners
    uint32_t owner_idx = 0;
    s_flist_node* owner_node = certificate_data->poolRegistration.poolOwners;
    while (owner_node != NULL) {
        tx_certificate_list_item_t* owner_item =
            (tx_certificate_list_item_t*) owner_node;
        ext_credential_t* owner_cred = &owner_item->certificate_data.stakeCredential;

        // Convert ext_credential to pool_owner for policy check
        pool_owner_t pool_owner = {
            .keyReferenceType = (owner_cred->type == EXT_CREDENTIAL_KEY_PATH) ?
                                KEY_REFERENCE_PATH : KEY_REFERENCE_HASH
        };
        if (owner_cred->type == EXT_CREDENTIAL_KEY_PATH) {
            pool_owner.path = owner_cred->keyPath;
        } else {
            memcpy(pool_owner.keyHash, owner_cred->keyHash, ADDRESS_KEY_HASH_LENGTH);
        }

        // Check owner security policy
        security_policy_t owner_policy = policyForSignTxStakePoolRegistrationOwner(
            G_context.tx_info.transaction.txSigningMode,
            &pool_owner
        );
        LEDGER_ASSERT(owner_policy != POLICY_DENY, "Pool owner security policy denied");

        if (owner_policy == POLICY_SHOW) {
            status = addCredentialUIPairs(
                owner_cred,
                "Owner",                        // KEY_PATH label
                "Owner",                        // KEY_HASH label
                "stake_vkh",                    // KEY_HASH bech32 prefix
                "Owner",                        // SCRIPT_HASH label
                "script"                        // SCRIPT_HASH bech32 prefix
            );
            if (status != SWO_SUCCESS) {
                return status;
            }
        }

        owner_node = owner_node->next;
        owner_idx++;
    }

    ASSERT(owner_idx == pool_owner_counts.total_owners);
    if (pool_owner_counts.total_owners == 0) {
        warning_bits_set(&G_context.tx_info.warning_bits,
                            WARNING_BIT_POOL_REGISTRATION_NO_OWNERS);
        const char *owners_value = "None";
        size_t owners_value_len = strlen(owners_value);
        size_t owners_buf_size = owners_value_len + 1;
        char *owners_none = ui_alloc_temp(owners_buf_size);
        if (owners_none == NULL) {
            return SWO_INSUFFICIENT_MEMORY;
        }
        strncpy(owners_none, owners_value, owners_buf_size);
        status = ui_pairs_add_static_label(UI_STATIC_LABEL("Pool owners"), owners_none) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
        if (status != SWO_SUCCESS) {
            return status;
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
                char *relay_index_str = ui_alloc_temp(MAX_RELAY_INDEX_STRING_LENGTH + 2);
                if (relay_index_str == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                snprintf(relay_index_str, MAX_RELAY_INDEX_STRING_LENGTH + 2, "#%u", relay_idx + 1);
                LEDGER_ASSERT(strlen(relay_index_str) <= MAX_RELAY_INDEX_STRING_LENGTH, "Relay index ui string buffer too short");
                status = ui_pairs_add_static_label(UI_STATIC_LABEL("Relay"), relay_index_str) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                if (status != SWO_SUCCESS) {
                    return status;
                }

                // Display relay format and details
                switch (relay->format) {
                    case RELAY_SINGLE_HOST_IP: {
                        // Display IPv4 if present
                        if (!relay->ipv4.isNull) {
                            char *ipv4_str = ui_alloc_temp(MAX_IPV4_STR_LENGTH + 2);
                            if (ipv4_str == NULL) return SWO_INSUFFICIENT_MEMORY;
                            inet_ntop4(relay->ipv4.ip, ipv4_str, MAX_IPV4_STR_LENGTH + 2);
                            LEDGER_ASSERT(strlen(ipv4_str) <= MAX_IPV4_STR_LENGTH, "IPv4 buffer overflow");
                            status = ui_pairs_add_static_label(UI_STATIC_LABEL("IPv4"), ipv4_str) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                            if (status != SWO_SUCCESS) {
                                return status;
                            }
                        }

                        // Display IPv6 if present
                        if (!relay->ipv6.isNull) {
                            char *ipv6_str = ui_alloc_temp(MAX_IPV6_STR_LENGTH + 2);
                            if (ipv6_str == NULL) return SWO_INSUFFICIENT_MEMORY;
                            inet_ntop6(relay->ipv6.ip, ipv6_str, MAX_IPV6_STR_LENGTH + 2);
                            LEDGER_ASSERT(strlen(ipv6_str) <= MAX_IPV6_STR_LENGTH, "IPv6 buffer overflow");
                            status = ui_pairs_add_static_label(UI_STATIC_LABEL("IPv6"), ipv6_str) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                            if (status != SWO_SUCCESS) {
                                return status;
                            }
                        }

                        // Display port if present
                        if (!relay->port.isNull) {
                            char *port_str = ui_alloc_temp(MAX_UINT64_STRING_LENGTH + 2);
                            if (port_str == NULL) return SWO_INSUFFICIENT_MEMORY;
                            snprintf(port_str, MAX_UINT64_STRING_LENGTH + 2, "%u", relay->port.number);
                            LEDGER_ASSERT(strlen(port_str) <= MAX_UINT64_STRING_LENGTH, "Port ui string buffer too short");
                            status = ui_pairs_add_static_label(UI_STATIC_LABEL("Port"), port_str) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                            if (status != SWO_SUCCESS) {
                                return status;
                            }
                        }
                        break;
                    }
                    case RELAY_SINGLE_HOST_NAME: {
                        // Display DNS name
                        if (relay->dnsNameSize > 0) {
                            char *dns_str = ui_alloc_temp(relay->dnsNameSize + 2);
                            if (dns_str == NULL) return SWO_INSUFFICIENT_MEMORY;
                            memcpy(dns_str, relay->dnsName, relay->dnsNameSize);
                            dns_str[relay->dnsNameSize] = '\0';
                            LEDGER_ASSERT(strlen(dns_str) == relay->dnsNameSize, "DNS name length mismatch");
                            status = ui_pairs_add_static_label(UI_STATIC_LABEL("DNS name"), dns_str) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                            if (status != SWO_SUCCESS) {
                                return status;
                            }
                        }

                        // Display port if present
                        if (!relay->port.isNull) {
                            char *port_str = ui_alloc_temp(MAX_UINT64_STRING_LENGTH + 2);
                            if (port_str == NULL) return SWO_INSUFFICIENT_MEMORY;
                            snprintf(port_str, MAX_UINT64_STRING_LENGTH + 2, "%u", relay->port.number);
                            LEDGER_ASSERT(strlen(port_str) <= MAX_UINT64_STRING_LENGTH, "Port ui string buffer too short");
                            status = ui_pairs_add_static_label(UI_STATIC_LABEL("Port"), port_str) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                            if (status != SWO_SUCCESS) {
                                return status;
                            }
                        }
                        break;
                    }
                    case RELAY_MULTIPLE_HOST_NAME: {
                        // Display DNS name (SRV record)
                        if (relay->dnsNameSize > 0) {
                            char *dns_str = ui_alloc_temp(relay->dnsNameSize + 2);
                            if (dns_str == NULL) return SWO_INSUFFICIENT_MEMORY;
                            memcpy(dns_str, relay->dnsName, relay->dnsNameSize);
                            dns_str[relay->dnsNameSize] = '\0';
                            LEDGER_ASSERT(strlen(dns_str) == relay->dnsNameSize, "SRV DNS name length mismatch");
                            status = ui_pairs_add_static_label(UI_STATIC_LABEL("SRV DNS"), dns_str) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                            if (status != SWO_SUCCESS) {
                                return status;
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

    ASSERT(relay_idx ==
            certificate_data->poolRegistration.numRelays);
    if (relay_idx == 0 &&
        G_context.tx_info.transaction.txSigningMode ==
            SIGN_TX_SIGNINGMODE_POOL_REGISTRATION_OPERATOR) {
        warning_bits_set(&G_context.tx_info.warning_bits,
                            WARNING_BIT_POOL_REGISTRATION_NO_RELAYS);
        const char *relays_value = "None";
        size_t relays_value_len = strlen(relays_value);
        size_t relays_buf_size = relays_value_len + 1;
        char *relays_none = ui_alloc_temp(relays_buf_size);
        if (relays_none == NULL) {
            return SWO_INSUFFICIENT_MEMORY;
        }
        strncpy(relays_none, relays_value, relays_buf_size);
        status = ui_pairs_add_static_label(UI_STATIC_LABEL("Pool relays"), relays_none) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
        if (status != SWO_SUCCESS) {
            return status;
        }
    }

    // Display metadata status with appropriate security policy
    if (certificate_data->poolRegistration.poolMetadataIsNull) {
        security_policy_t no_metadata_policy = policyForSignTxStakePoolRegistrationNoMetadata();
        LEDGER_ASSERT(no_metadata_policy != POLICY_DENY, "No metadata security policy denied");

        if (no_metadata_policy == POLICY_SHOW) {
            const char *none_str = "none (anonymous pool)";
            char *metadata_none = ui_alloc_temp(strlen(none_str) + 1);
            if (metadata_none == NULL) return SWO_INSUFFICIENT_MEMORY;
            memcpy(metadata_none, none_str, strlen(none_str) + 1);
            status = ui_pairs_add_static_label(UI_STATIC_LABEL("Metadata"), metadata_none) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
            if (status != SWO_SUCCESS) {
                return status;
            }
        }
    } else {
        security_policy_t metadata_policy = policyForSignTxStakePoolRegistrationMetadata();
        LEDGER_ASSERT(metadata_policy != POLICY_DENY, "Metadata security policy denied");

        if (metadata_policy == POLICY_SHOW) {
            char *metadata_url = ui_alloc_temp(
                certificate_data->poolRegistration.poolMetadata.urlSize + 1
            );
            if (metadata_url == NULL) return SWO_INSUFFICIENT_MEMORY;
            memcpy(metadata_url,
                    certificate_data->poolRegistration.poolMetadata.url,
                    certificate_data->poolRegistration.poolMetadata.urlSize);
            metadata_url[certificate_data->poolRegistration.poolMetadata.urlSize] = '\0';
            status = ui_pairs_add_static_label(UI_STATIC_LABEL("Pool metadata url"), metadata_url) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
            if (status != SWO_SUCCESS) {
                return status;
            }

            char *metadata_hash_tmp = ui_alloc_temp(MAX_POOL_METADATA_HASH_STRING_LENGTH + 1);
            if (metadata_hash_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;
            int hex_status = bytes_to_lowercase_hex(
                metadata_hash_tmp,
                MAX_POOL_METADATA_HASH_STRING_LENGTH + 1,
                certificate_data->poolRegistration.poolMetadata.hash,
                POOL_METADATA_HASH_LENGTH);
            LEDGER_ASSERT(hex_status == 0, "Pool metadata hash formatting failed");
            status = ui_pairs_add_static_label(UI_STATIC_LABEL("Pool metadata hash"), metadata_hash_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
            if (status != SWO_SUCCESS) {
                return status;
            }
        }
    }
    return SWO_SUCCESS;
}

static int ui_strings_certificates(transaction_t *tx) {
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
                char *cert_num_tmp = ui_alloc_temp(MAX_UINT64_STRING_LENGTH + 2);
                if (cert_num_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                snprintf(cert_num_tmp, MAX_UINT64_STRING_LENGTH + 2, "#%d", certificate_num);
                LEDGER_ASSERT(strlen(cert_num_tmp) <= MAX_UINT64_STRING_LENGTH, "Certificate number ui string buffer too short");
                int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Certificate"), cert_num_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                if (status != SWO_SUCCESS) {
                    return status;
                }

                // Certificate type
                const char *cert_type_name = getCertificateTypeName(certificate_item->certificate_data.type);
                size_t cert_type_len = strlen(cert_type_name);
                char *cert_type_tmp = ui_alloc_temp(cert_type_len + 1);
                if (cert_type_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                memcpy(cert_type_tmp, cert_type_name, cert_type_len + 1);
                status = ui_pairs_add_static_label(UI_STATIC_LABEL("Type"), cert_type_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                if (status != SWO_SUCCESS) {
                    return status;
                }

                // Certificate-specific fields
                switch (certificate_item->certificate_data.type) {
                    case CERTIFICATE_STAKE_REGISTRATION:
                    case CERTIFICATE_STAKE_DEREGISTRATION: {
                        status = ui_strings_certificate_stake_registration(&certificate_item->certificate_data);
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    case CERTIFICATE_STAKE_DELEGATION: {
                        status = ui_strings_certificate_stake_delegation(&certificate_item->certificate_data);
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    case CERTIFICATE_STAKE_REGISTRATION_CONWAY:
                    case CERTIFICATE_STAKE_DEREGISTRATION_CONWAY: {
                        status = ui_strings_certificate_stake_conway(&certificate_item->certificate_data);
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    case CERTIFICATE_STAKE_POOL_RETIREMENT: {
                        status = ui_strings_certificate_pool_retirement(&certificate_item->certificate_data);
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    case CERTIFICATE_VOTE_DELEGATION: {
                        status = ui_strings_certificate_vote_delegation(&certificate_item->certificate_data);
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    case CERTIFICATE_AUTHORIZE_COMMITTEE_HOT: {
                        status = ui_strings_certificate_committee_hot(&certificate_item->certificate_data);
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    case CERTIFICATE_RESIGN_COMMITTEE_COLD: {
                        status = ui_strings_certificate_committee_resign(&certificate_item->certificate_data);
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    case CERTIFICATE_DREP_REGISTRATION: {
                        status = ui_strings_certificate_drep_registration(&certificate_item->certificate_data);
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    case CERTIFICATE_DREP_DEREGISTRATION: {
                        status = ui_strings_certificate_drep_deregistration(&certificate_item->certificate_data);
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    case CERTIFICATE_DREP_UPDATE: {
                        status = ui_strings_certificate_drep_update(&certificate_item->certificate_data);
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    case CERTIFICATE_STAKE_POOL_REGISTRATION: {
                        status = ui_strings_certificate_pool_registration(&certificate_item->certificate_data, tx->txSigningMode);
                        if (status != SWO_SUCCESS) {
                            return status;
                        }
                        break;
                    }

                    default:
                        LEDGER_ASSERT(false, "Unknown certificate type");
                }

                certificate_num++;
            }
            break;
            case POLICY_HIDE:
                break;
        }

        app_mem_free(certificate_item);
        certificate_node = next;
    }
    tx->certificates = NULL;

    return SWO_SUCCESS;
}

static int ui_strings_withdrawals(transaction_t *tx) {
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
                char *withdrawal_num_tmp = ui_alloc_temp(MAX_UINT64_STRING_LENGTH + 2);
                if (withdrawal_num_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                snprintf(withdrawal_num_tmp, MAX_UINT64_STRING_LENGTH + 2, "#%d", withdrawal_num);
                LEDGER_ASSERT(strlen(withdrawal_num_tmp) <= MAX_UINT64_STRING_LENGTH, "Withdrawal number ui string buffer too short");
                int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Withdrawal"), withdrawal_num_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                if (status != SWO_SUCCESS) {
                    return status;
                }

                char *withdrawal_amount_tmp = ui_alloc_temp(MAX_ADA_AMOUNT_STRING_LENGTH + 1);
                if (withdrawal_amount_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                bool withdrawal_amount_formatted =
                    str_formatAdaAmount(withdrawal_item->withdrawal_data.amount,
                                        withdrawal_amount_tmp,
                                        MAX_ADA_AMOUNT_STRING_LENGTH + 1);
                ASSERT(withdrawal_amount_formatted);
                status = ui_pairs_add_static_label(UI_STATIC_LABEL("Amount"), withdrawal_amount_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                if (status != SWO_SUCCESS) {
                    return status;
                }

                status = addRewardAccountUIPairs(
                    G_context.tx_info.transaction.networkId,
                    &withdrawal_item->withdrawal_data.stakeCredential
                );
                if (status != SWO_SUCCESS) {
                    return status;
                }

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

    return SWO_SUCCESS;
}

static int ui_strings_aux_data_hash(transaction_t *tx) {
    if (tx->includeAuxDataHash) {
        security_policy_t policy = policyForSignTxAuxData(tx->auxDataType);
        LEDGER_ASSERT(policy != POLICY_DENY, "Aux data denied during UI");
        if (policy == POLICY_SHOW) {
            char *hash_tmp = ui_alloc_temp(MAX_TX_HASH_DISPLAY_LENGTH + 1);
            if (hash_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;
            int hex_status = bytes_to_lowercase_hex(hash_tmp, MAX_TX_HASH_DISPLAY_LENGTH + 1, tx->auxDataHash, AUX_DATA_HASH_LENGTH);
            LEDGER_ASSERT(hex_status == 0, "Aux data hash hex formatting failed");
            int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Auxiliary data hash"), hash_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
            if (status != SWO_SUCCESS) return status;
        }
    }
    return SWO_SUCCESS;
}

static int ui_strings_validity_interval_start(transaction_t *tx) {
    if (!tx->includeValidityIntervalStart) {
        return SWO_SUCCESS;
    }
    security_policy_t validity_interval_start_policy = policyForSignTxValidityIntervalStart();
    LEDGER_ASSERT(validity_interval_start_policy != POLICY_DENY, "Validity interval start denied during UI");
    switch (validity_interval_start_policy) {
        case POLICY_DENY:
            // Already asserted above, this case should never be reached
            break;
        case POLICY_SHOW: {
            char *validity_interval_start_tmp = ui_alloc_temp(MAX_VALIDITY_BOUNDARY_STRING_LENGTH + 1);
            if (validity_interval_start_tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            bool vis_formatted = str_formatValidityBoundary(tx->validityIntervalStart,
                                                            tx->networkId,
                                                            tx->protocolMagic,
                                                            validity_interval_start_tmp,
                                                            MAX_VALIDITY_BOUNDARY_STRING_LENGTH + 1);
            ASSERT(vis_formatted);
            int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Validity interval start"), validity_interval_start_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
            if (status != SWO_SUCCESS) {
                return status;
            }
            break;
        }
        case POLICY_HIDE:
            break;
    }
    return SWO_SUCCESS;
}

static int ui_strings_mint(transaction_t *tx) {
    if (tx->num_mint_asset_groups > 0) {
        security_policy_t mint_policy = policyForSignTxMintInit(tx->txSigningMode);
        LEDGER_ASSERT(mint_policy != POLICY_DENY, "Mint denied during UI");
        if (mint_policy == POLICY_SHOW) {
            char *summary_tmp = ui_alloc_temp(MAX_MINT_SUMMARY_STRING_LENGTH + 2);
            if (summary_tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            snprintf(summary_tmp,
                     MAX_MINT_SUMMARY_STRING_LENGTH + 2,
                     "%u asset group%s",
                     tx->num_mint_asset_groups,
                     (tx->num_mint_asset_groups == 1) ? "" : "s");
            LEDGER_ASSERT(strlen(summary_tmp) <= MAX_MINT_SUMMARY_STRING_LENGTH, "Mint summary ui string buffer too short");
            int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Mint"), summary_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
            if (status != SWO_SUCCESS) {
                return status;
            }

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

                    char *fingerprint_tmp = ui_alloc_temp(MAX_TOKEN_FINGERPRINT_STRING_LENGTH + 1);
                    if (fingerprint_tmp == NULL) {
                        return SWO_INSUFFICIENT_MEMORY;
                    }
                    size_t fingerprint_len = deriveAssetFingerprintBech32(
                        tokenGroup.policyId,
                        sizeof(tokenGroup.policyId),
                        token->assetName,
                        token->assetNameLen,
                        fingerprint_tmp,
                        MAX_TOKEN_FINGERPRINT_STRING_LENGTH + 1);
                    LEDGER_ASSERT(fingerprint_len > 0, "Fingerprint derivation failed");
                    status = ui_pairs_add_static_label(UI_STATIC_LABEL("Mint fingerprint"), fingerprint_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                    if (status != SWO_SUCCESS) {
                        return status;
                    }

                    char *amount_tmp = ui_alloc_temp(MAX_MINT_AMOUNT_STRING_LENGTH + 1);
                    if (amount_tmp == NULL) {
                        return SWO_INSUFFICIENT_MEMORY;
                    }
                    bool mint_amount_formatted = str_formatTokenAmountMint(&tokenGroup,
                                                                           token->assetName,
                                                                           token->assetNameLen,
                                                                           token->amount,
                                                                           amount_tmp,
                                                                           MAX_MINT_AMOUNT_STRING_LENGTH + 1);
                    ASSERT(mint_amount_formatted);
                    status = ui_pairs_add_static_label(UI_STATIC_LABEL("Mint amount"), amount_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                    if (status != SWO_SUCCESS) {
                        return status;
                    }

                    // Free token node immediately after UI strings are materialized
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
    return SWO_SUCCESS;
}

static int ui_strings_script_data_hash(transaction_t *tx) {
    if (tx->includeScriptDataHash) {
        security_policy_t policy = policyForSignTxScriptDataHash(tx->txSigningMode);
        LEDGER_ASSERT(policy != POLICY_DENY, "Script data hash denied during UI");
        if (policy == POLICY_SHOW) {
            char *hash_tmp = ui_alloc_temp(MAX_TX_HASH_DISPLAY_LENGTH + 1);
            if (hash_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;
            int hex_status = bytes_to_lowercase_hex(hash_tmp, MAX_TX_HASH_DISPLAY_LENGTH + 1, tx->scriptDataHash, SCRIPT_DATA_HASH_LENGTH);
            LEDGER_ASSERT(hex_status == 0, "Script data hash hex formatting failed");
            int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Script data hash"), hash_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
            if (status != SWO_SUCCESS) return status;
        }
    }
    return SWO_SUCCESS;
}

static int ui_strings_collateral_inputs(transaction_t *tx) {
    if (tx->num_collateral_inputs == 0) {
        return SWO_SUCCESS;
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
            char *input_tmp = ui_alloc_temp(MAX_INPUT_DISPLAY_STRING_LENGTH + 1);
            if (input_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;
            int status = format_input_with_index(input_tmp, MAX_INPUT_DISPLAY_STRING_LENGTH + 1, &input_item->input_data);
            if (status != SWO_SUCCESS) return status;
            status = ui_pairs_add_static_label(UI_STATIC_LABEL("Coll input"), input_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
            if (status != SWO_SUCCESS) return status;
        }

        app_mem_free(input_item);
        collateral_input_node = next;
    }
    tx->collateral_inputs = NULL;
    return SWO_SUCCESS;
}

static int ui_strings_required_signers(transaction_t *tx) {
    if (tx->num_required_signers > 0) {
        s_flist_node *req_signer_node = tx->required_signers;
        while (req_signer_node != NULL) {
            tx_required_signer_list_item_t *item = (tx_required_signer_list_item_t *) req_signer_node;
            s_flist_node *next = req_signer_node->next;

            security_policy_t policy = policyForSignTxRequiredSigner(tx->txSigningMode, &item->required_signer_data);
            LEDGER_ASSERT(policy != POLICY_DENY, "Required signer denied during UI");

            if (policy == POLICY_SHOW) {
                char *value_tmp = ui_alloc_temp(MAX_BECH32_STRING_LENGTH + 1);
                if (value_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;

                if (item->required_signer_data.type == REQUIRED_SIGNER_WITH_HASH) {
                     bool encoded = format_bech32("vkh", item->required_signer_data.keyHash, ADDRESS_KEY_HASH_LENGTH, value_tmp, MAX_BECH32_STRING_LENGTH + 1);
                     LEDGER_ASSERT(encoded, "Unable to format required signer key hash");
                } else {
                     bool formatted = format_bip44_path(&item->required_signer_data.keyPath, value_tmp, MAX_BIP44_PATH_STRING_LENGTH + 1);
                     LEDGER_ASSERT(formatted, "Unable to format required signer path");
                }

                int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Required signer"), value_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                if (status != SWO_SUCCESS) return status;
            }
            app_mem_free(item);
            req_signer_node = next;
        }
    }
    tx->required_signers = NULL;
    return SWO_SUCCESS;
}

static int ui_strings_collateral_output(transaction_t *tx) {
    if (!tx->includeCollateralOutput) {
        return SWO_SUCCESS;
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

    int status = SWO_SUCCESS;

    if (collateral_policy == POLICY_SHOW) {
        char *collateral_address_tmp = ui_alloc_temp(MAX_HUMAN_ADDRESS_LENGTH + 1);
        if (collateral_address_tmp == NULL) {
            return SWO_INSUFFICIENT_MEMORY;
        }

        bool collateral_address_formatted = false;
        if (collateral_desc.destination.type == DESTINATION_THIRD_PARTY) {
            collateral_address_formatted = format_address_human_readable(
                collateral_desc.destination.address.buffer,
                collateral_desc.destination.address.size,
                collateral_address_tmp,
                MAX_HUMAN_ADDRESS_LENGTH + 1);
        } else {
            uint8_t address_bytes[MAX_ADDRESS_LENGTH] = {0};
            size_t derived_len = deriveAddress(
                collateral_desc.destination.params,
                address_bytes,
                sizeof(address_bytes));
            if (derived_len > 0) {
                collateral_address_formatted = format_address_human_readable(
                    address_bytes,
                    derived_len,
                    collateral_address_tmp,
                    MAX_HUMAN_ADDRESS_LENGTH + 1);
            }
        }
        LEDGER_ASSERT(collateral_address_formatted, "Collateral address formatting failed");
        status = ui_pairs_add_static_label(UI_STATIC_LABEL("Collateral address"), collateral_address_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
        if (status != SWO_SUCCESS) {
            return status;
        }

        if (collateral_ada_policy == POLICY_SHOW) {
            char *amount_tmp = ui_alloc_temp(MAX_ADA_AMOUNT_STRING_LENGTH + 1);
            if (amount_tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            bool amount_formatted = str_formatAdaAmount(collateral_desc.amount,
                                                        amount_tmp,
                                                        MAX_ADA_AMOUNT_STRING_LENGTH + 1);
            ASSERT(amount_formatted);
            status = ui_pairs_add_static_label(UI_STATIC_LABEL("Collateral amount"), amount_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
            if (status != SWO_SUCCESS) {
                return status;
            }
        }

        if (collateral_confirm_policy == POLICY_SHOW) {
            warning_bits_set(&G_context.tx_info.warning_bits, WARNING_BIT_COLLATERAL_OUTPUT_WARNING);
        }
    }

    status = ui_materialize_token_groups(
        tx->collateral_output.assetGroups,
        tx->collateral_output.numAssetGroups,
        show_collateral_tokens);
    if (status != SWO_SUCCESS) {
        return status;
    }

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

    return SWO_SUCCESS;
}

static int ui_strings_total_collateral(transaction_t *tx) {
    if (!tx->includeTotalCollateral) {
        return SWO_SUCCESS;
    }
    security_policy_t policy = policyForSignTxTotalCollateral();
    LEDGER_ASSERT(policy != POLICY_DENY, "Total collateral denied during UI");
    if (policy == POLICY_SHOW) {
        char *amount_tmp = ui_alloc_temp(MAX_ADA_AMOUNT_STRING_LENGTH + 1);
        if (amount_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;
        bool formatted = str_formatAdaAmount(tx->totalCollateral, amount_tmp, MAX_ADA_AMOUNT_STRING_LENGTH + 1);
        ASSERT(formatted);
        int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Total collateral"), amount_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
        if (status != SWO_SUCCESS) return status;
    }
    return SWO_SUCCESS;
}

static int ui_strings_reference_inputs(transaction_t *tx) {
    if (tx->num_reference_inputs == 0) {
        return SWO_SUCCESS;
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
            char *input_tmp = ui_alloc_temp(MAX_INPUT_DISPLAY_STRING_LENGTH + 1);
            if (input_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;
            int status = format_input_with_index(input_tmp, MAX_INPUT_DISPLAY_STRING_LENGTH + 1, &input_item->input_data);
            if (status != SWO_SUCCESS) return status;
            status = ui_pairs_add_static_label(UI_STATIC_LABEL("Ref input"), input_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
            if (status != SWO_SUCCESS) return status;
        }

        app_mem_free(input_item);
        reference_input_node = next;
    }
    tx->reference_inputs = NULL;
    return SWO_SUCCESS;
}

static int ui_strings_voting_procedures(transaction_t *tx) {
    if (tx->num_voters > 0) {
        s_flist_node *voter_node = tx->voting_procedures;
        while (voter_node != NULL) {
            voter_votes_list_item_t *voter_item = (voter_votes_list_item_t *) voter_node;
            s_flist_node *voter_next = voter_node->next;

            security_policy_t policy = policyForSignTxVotingProcedure(tx->txSigningMode, &voter_item->voter_votes_data.voter);
            LEDGER_ASSERT(policy != POLICY_DENY, "Voting procedure denied during UI");

            if (policy == POLICY_SHOW) {
                // Display Voter
                int status = addVoterUIPairs(&voter_item->voter_votes_data.voter);
                if (status != SWO_SUCCESS) return status;

                // Iterate votes
                s_flist_node *vote_node = voter_item->voter_votes_data.votes;
                while (vote_node != NULL) {
                    vote_list_item_t *vote_item = (vote_list_item_t *) vote_node;

                    // Gov Action Tx Hash
                    char *gov_action_hash_tmp = ui_alloc_temp(MAX_TX_HASH_DISPLAY_LENGTH + 1);
                    if (gov_action_hash_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;

                    int hex_status = bytes_to_lowercase_hex(gov_action_hash_tmp, MAX_TX_HASH_DISPLAY_LENGTH + 1, vote_item->vote_data.govActionId.txHash, TX_HASH_LENGTH);
                    LEDGER_ASSERT(hex_status == 0, "Gov action hash hex formatting failed");

                    status = ui_pairs_add_static_label(UI_STATIC_LABEL("Gov action tx hash"), gov_action_hash_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                    if (status != SWO_SUCCESS) return status;

                    // Gov Action Index
                    char *gov_action_index_tmp = ui_alloc_temp(MAX_UINT64_STRING_LENGTH + 2);
                    if (gov_action_index_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;

                    snprintf(gov_action_index_tmp, MAX_UINT64_STRING_LENGTH + 2, "%u", vote_item->vote_data.govActionId.govActionIndex);
                    LEDGER_ASSERT(strlen(gov_action_index_tmp) <= MAX_UINT64_STRING_LENGTH, "Gov action index ui string buffer too short");

                    status = ui_pairs_add_static_label(UI_STATIC_LABEL("Gov action index"), gov_action_index_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                    if (status != SWO_SUCCESS) return status;

                    // Vote Option
                    const char* vote_str;
                    switch (vote_item->vote_data.voteOption) {
                        case VOTE_NO: vote_str = "No"; break;
                        case VOTE_YES: vote_str = "Yes"; break;
                        case VOTE_ABSTAIN: vote_str = "Abstain"; break;
                        default: vote_str = "Unknown"; break;
                    }
                    char *vote_str_tmp = ui_alloc_temp(MAX_VOTE_OPTION_LENGTH + 1);
                    if (vote_str_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;
                    strncpy(vote_str_tmp, vote_str, MAX_VOTE_OPTION_LENGTH + 1);
                    status = ui_pairs_add_static_label(UI_STATIC_LABEL("Vote"), vote_str_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                    if (status != SWO_SUCCESS) return status;

                    // Anchor
                    status = addAnchorUIPairs(&vote_item->vote_data.anchor);
                    if (status != SWO_SUCCESS) return status;

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
    return SWO_SUCCESS;
}

static int ui_strings_treasury(transaction_t *tx) {
    if (tx->includeTreasury) {
        security_policy_t policy = policyForSignTxTreasury(tx->txSigningMode, tx->treasury);
        LEDGER_ASSERT(policy != POLICY_DENY, "Treasury denied during UI");
        if (policy == POLICY_SHOW) {
            char *tmp = ui_alloc_temp(MAX_ADA_AMOUNT_STRING_LENGTH + 1);
            if (tmp == NULL) return SWO_INSUFFICIENT_MEMORY;
            bool formatted = str_formatAdaAmount(tx->treasury, tmp, MAX_ADA_AMOUNT_STRING_LENGTH + 1);
            ASSERT(formatted);
            int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Treasury"), tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
            if (status != SWO_SUCCESS) return status;
        }
    }
    return SWO_SUCCESS;
}

static int ui_strings_donation(transaction_t *tx) {
    if (tx->includeDonation) {
        security_policy_t policy = policyForSignTxDonation(tx->txSigningMode, tx->donation);
        LEDGER_ASSERT(policy != POLICY_DENY, "Donation denied during UI");
        if (policy == POLICY_SHOW) {
            char *tmp = ui_alloc_temp(MAX_ADA_AMOUNT_STRING_LENGTH + 1);
            if (tmp == NULL) return SWO_INSUFFICIENT_MEMORY;
            bool formatted = str_formatAdaAmount(tx->donation, tmp, MAX_ADA_AMOUNT_STRING_LENGTH + 1);
            ASSERT(formatted);
            int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Donation"), tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
            if (status != SWO_SUCCESS) return status;
        }
    }
    return SWO_SUCCESS;
}

static int ui_strings_tx_hash(void) {
    if (G_context.tx_info.raw_tx != NULL) {
        app_mem_free(G_context.tx_info.raw_tx);
        G_context.tx_info.raw_tx = NULL;
        G_context.tx_info.raw_tx_len = 0;
    }

    char *hash_tmp = ui_alloc_temp(MAX_TX_HASH_DISPLAY_LENGTH + 1);
    if (hash_tmp == NULL) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    int hex_status = bytes_to_lowercase_hex(hash_tmp,
                                            MAX_TX_HASH_DISPLAY_LENGTH + 1,
                                            G_context.tx_info.tx_hash,
                                            sizeof(G_context.tx_info.tx_hash));
    LEDGER_ASSERT(hex_status == 0, "Tx hash hex formatting failed");
    int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Transaction hash"), hash_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
    if (status != SWO_SUCCESS) {
        return status;
    }
    return SWO_SUCCESS;
}

static int add_ui_strings_and_free_parsed_data(void) {
    LEDGER_ASSERT(G_context.state.tx_state == TX_STATE_HASHED, "String materialization invoked too early");
    transaction_t *tx = &G_context.tx_info.transaction;
    int status;

    TRACE("UI materialization starting");

    status = ui_strings_inputs(tx);
    if (status != SWO_SUCCESS) return status;
    status = ui_strings_outputs(tx);
    if (status != SWO_SUCCESS) return status;
    status = ui_strings_fee(tx);
    if (status != SWO_SUCCESS) return status;
    status = ui_strings_ttl(tx);
    if (status != SWO_SUCCESS) return status;
    status = ui_strings_certificates(tx);
    if (status != SWO_SUCCESS) return status;
    status = ui_strings_withdrawals(tx);
    if (status != SWO_SUCCESS) return status;
    status = ui_strings_aux_data_hash(tx);
    if (status != SWO_SUCCESS) return status;
    status = ui_strings_validity_interval_start(tx);
    if (status != SWO_SUCCESS) return status;
    status = ui_strings_mint(tx);
    if (status != SWO_SUCCESS) return status;
    status = ui_strings_script_data_hash(tx);
    if (status != SWO_SUCCESS) return status;
    status = ui_strings_collateral_inputs(tx);
    if (status != SWO_SUCCESS) return status;
    status = ui_strings_required_signers(tx);
    if (status != SWO_SUCCESS) return status;
    status = ui_strings_collateral_output(tx);
    if (status != SWO_SUCCESS) return status;
    status = ui_strings_total_collateral(tx);
    if (status != SWO_SUCCESS) return status;
    status = ui_strings_reference_inputs(tx);
    if (status != SWO_SUCCESS) return status;
    status = ui_strings_voting_procedures(tx);
    if (status != SWO_SUCCESS) return status;
    status = ui_strings_treasury(tx);
    if (status != SWO_SUCCESS) return status;
    status = ui_strings_donation(tx);
    if (status != SWO_SUCCESS) return status;
    status = ui_strings_tx_hash();
    if (status != SWO_SUCCESS) return status;

    TRACE("UI materialization complete");
    return SWO_SUCCESS;
}

static int ui_build_transaction_warnings(void) {
    const warning_definition_t *warning_defs[WARNING_BIT_COUNT];
    size_t warning_count =
        warning_bits_to_definitions(G_context.tx_info.warning_bits, warning_defs, WARNING_BIT_COUNT);
    if (warning_count == 0) {
        g_warning = NULL;
        return SWO_SUCCESS;
    }

    const nbgl_icon_details_t **icons =
        (const nbgl_icon_details_t **) ui_mem_alloc(sizeof(nbgl_icon_details_t *) * warning_count);
    const char **titles = (const char **) ui_mem_alloc(sizeof(const char *) * warning_count);
    const char **subtexts = (const char **) ui_mem_alloc(sizeof(const char *) * warning_count);
    nbgl_warningDetails_t *details =
        (nbgl_warningDetails_t *) ui_mem_alloc(sizeof(nbgl_warningDetails_t) * warning_count);
    nbgl_warningDetails_t *intro = (nbgl_warningDetails_t *) ui_mem_alloc(sizeof(nbgl_warningDetails_t));
    nbgl_warningDetails_t *review = (nbgl_warningDetails_t *) ui_mem_alloc(sizeof(nbgl_warningDetails_t));
    nbgl_contentCenter_t *info = (nbgl_contentCenter_t *) ui_mem_alloc(sizeof(nbgl_contentCenter_t));
    g_warning = (nbgl_warning_t *) ui_mem_alloc(sizeof(nbgl_warning_t));

    if (icons == NULL || titles == NULL || subtexts == NULL || details == NULL || intro == NULL ||
        review == NULL || info == NULL || g_warning == NULL) {
        g_warning = NULL;
        return SWO_INSUFFICIENT_MEMORY;
    }

    for (size_t i = 0; i < warning_count; i++) {
        const warning_definition_t *def = (const warning_definition_t *) PIC(warning_defs[i]);
        const char *title = (const char *) PIC(def->title);
        const char *description = (const char *) PIC(def->description);
        titles[i] = title;
        subtexts[i] = description;
        icons[i] = &WARNING_ICON;

        details[i].title = title;
        details[i].type = CENTERED_INFO_WARNING;
        details[i].centeredInfo.icon = &WARNING_ICON;
        details[i].centeredInfo.title = title;
        details[i].centeredInfo.description = description;
    }

    const char *const warning_intro_title = (const char *) PIC("Security report");
    const char *const warning_review_title = (const char *) PIC("Warning details");
    const char *const warning_info_title = (const char *) PIC("Transaction warning");
    const char *const warning_info_desc = (const char *) PIC("Please review the security warnings before signing.");

    intro->title = warning_intro_title;
    intro->type = BAR_LIST_WARNING;
    intro->barList.nbBars = warning_count;
    intro->barList.icons = icons;
    intro->barList.texts = titles;
    intro->barList.subTexts = subtexts;
    intro->barList.details = details;

    review->title = warning_review_title;
    review->type = BAR_LIST_WARNING;
    review->barList.nbBars = warning_count;
    review->barList.icons = icons;
    review->barList.texts = titles;
    review->barList.subTexts = subtexts;
    review->barList.details = details;

    info->icon = &WARNING_ICON;
    info->title = warning_info_title;
    info->description = warning_info_desc;

    g_warning->introDetails = intro;
    g_warning->reviewDetails = review;
    g_warning->info = info;
    g_warning->introTopRightIcon = &WARNING_ICON;
    g_warning->reviewTopRightIcon = &WARNING_ICON;

    return SWO_SUCCESS;
}

static inline bool status_requires_streaming(int status) {
    return status == SWO_INSUFFICIENT_MEMORY;
}

static int ui_build_pairs_and_warnings(void) {
    int status = add_ui_strings_and_free_parsed_data();
    if (status != SWO_SUCCESS) {
        return status;
    }
    return ui_build_transaction_warnings();
}

int ui_prepare_transaction_review(void) {
    if (G_context.req_type != REQUEST_SIGN_TRANSACTION) {
        return send_error_and_reset(SWO_BAD_STATE);
    }
    LEDGER_ASSERT(G_context.state.tx_state == TX_STATE_HASHED, "UI prep called too early");
    uint16_t pair_count = G_context.tx_info.planned_ui_pairs;

    // pair_count should never be 0 - at minimum we display fee
    LEDGER_ASSERT(pair_count > 0, "UI pair count is zero - at minimum fee must be displayed");

    // If pair count exceeds NBGL capability, reject the transaction
    if (pair_count > UINT8_MAX) {
        return send_error_and_reset(SWO_UI_PAIRS_EXCEED_CAPABILITY);
    }

    if (!ui_pairs_init((uint8_t) pair_count)) {
        return send_error_and_reset(SWO_INSUFFICIENT_MEMORY);
    }

    int status = ui_build_pairs_and_warnings();
    if (status != SWO_SUCCESS) {
        ui_pairs_cleanup();
        ui_clear_prepared_warning();
        if (status_requires_streaming(status)) {
            LEDGER_ASSERT(false, "Need streaming UI but not implemented (status=0x%04x)", status);
        }
        return status;
    }

    // Validate that the actual number of pairs materialized matches the planned count
    LEDGER_ASSERT(ui_pairs_get_count() == pair_count,
                  "UI pair count mismatch: planned %u but materialized %u",
                  pair_count, ui_pairs_get_count());

    return SWO_SUCCESS;
}

const nbgl_warning_t *ui_get_prepared_warning(void) {
    return g_warning;
}

void ui_clear_prepared_warning(void) {
    g_warning = NULL;
}
