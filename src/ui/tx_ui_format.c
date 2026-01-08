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

static bool format_input_with_index(char *out, size_t out_size, const tx_input_t *input) {
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

static int ui_format_token_groups(asset_group_t* assetGroups,
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
                char *fingerprint_tmp = (char *) app_mem_alloc(MAX_TOKEN_FINGERPRINT_STRING_LENGTH + 2);
                if (fingerprint_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                size_t fingerprint_len = deriveAssetFingerprintBech32(
                    group->policyId,
                    MINTING_POLICY_ID_LENGTH,
                    token->assetName,
                    token->assetNameLen,
                    fingerprint_tmp,
                    MAX_TOKEN_FINGERPRINT_STRING_LENGTH + 2);
                LEDGER_ASSERT(fingerprint_len > 0, "Fingerprint derivation failed");
                LEDGER_ASSERT(strlen(fingerprint_tmp) <= MAX_TOKEN_FINGERPRINT_STRING_LENGTH, "Fingerprint ui string buffer too short");
                int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Asset fingerprint"), fingerprint_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                if (status != SWO_SUCCESS) {
                    return status;
                }
                char *token_amount_tmp = (char *) app_mem_alloc(MAX_TOKEN_AMOUNT_OUTPUT_STRING_LENGTH + 2);
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
                    MAX_TOKEN_AMOUNT_OUTPUT_STRING_LENGTH + 2);
                LEDGER_ASSERT(token_amount_formatted, "Failed to format token amount");
                LEDGER_ASSERT(strlen(token_amount_tmp) <= MAX_TOKEN_AMOUNT_OUTPUT_STRING_LENGTH, "Token amount ui string buffer too short");
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
            char *input_tmp = (char *) app_mem_alloc(MAX_INPUT_DISPLAY_STRING_LENGTH + 2);
            if (input_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;
            bool input_formatted = format_input_with_index(input_tmp, MAX_INPUT_DISPLAY_STRING_LENGTH + 2, &input_item->input_data);
            LEDGER_ASSERT(input_formatted, "Failed to format input");
            LEDGER_ASSERT(strlen(input_tmp) <= MAX_INPUT_DISPLAY_STRING_LENGTH, "Input display ui string buffer too short");
            int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Input"), input_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
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
                char *output_num_tmp = (char *) app_mem_alloc(MAX_UINT64_STRING_LENGTH + 2);
                if (output_num_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                snprintf(output_num_tmp, MAX_UINT64_STRING_LENGTH + 2, "#%d", output_num);
                LEDGER_ASSERT(strlen(output_num_tmp) <= MAX_UINT64_STRING_LENGTH, "Output number ui string buffer too short");
                int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Output"), output_num_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                if (status != SWO_SUCCESS) {
                    return status;
                }
                char *address_tmp = (char *) app_mem_alloc(MAX_HUMAN_ADDRESS_LENGTH + 2);
                if (address_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }

                bool address_formatted = false;
                if (output_item->output_data.destination.type == DESTINATION_THIRD_PARTY) {
                    address_formatted = format_address_human_readable(
                        output_item->output_data.destination.address.buffer,
                        output_item->output_data.destination.address.size,
                        address_tmp,
                        MAX_HUMAN_ADDRESS_LENGTH + 2
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
                            MAX_HUMAN_ADDRESS_LENGTH + 2
                        );
                    }
                }

                LEDGER_ASSERT(address_formatted, "Address formatting failed");
                LEDGER_ASSERT(strlen(address_tmp) > 0, "Address length zero");
                LEDGER_ASSERT(strlen(address_tmp) <= MAX_HUMAN_ADDRESS_LENGTH, "Address ui string buffer too short");

                status = ui_pairs_add_static_label(UI_STATIC_LABEL("Address"), address_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                if (status != SWO_SUCCESS) {
                    return status;
                }

                // For device-owned addresses, show payment and staking details
                if (output_item->output_data.destination.type == DESTINATION_DEVICE_OWNED) {
                    status = addPaymentInfoUIPair(&output_item->output_data.destination.params);
                    if (status != SWO_SUCCESS) {
                        return status;
                    }
                    status = addStakingInfoUIPair(&output_item->output_data.destination.params);
                    if (status != SWO_SUCCESS) {
                        return status;
                    }
                }

                char *amount_tmp = (char *) app_mem_alloc(MAX_ADA_AMOUNT_STRING_LENGTH + 2);
                if (amount_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                bool amount_formatted = str_formatAdaAmount(output_item->output_data.adaAmount,
                                                            amount_tmp,
                                                            MAX_ADA_AMOUNT_STRING_LENGTH + 2);
                LEDGER_ASSERT(amount_formatted, "Failed to format amount");
                LEDGER_ASSERT(strlen(amount_tmp) <= MAX_ADA_AMOUNT_STRING_LENGTH, "Amount ui string buffer too short");
                status = ui_pairs_add_static_label(UI_STATIC_LABEL("Amount"), amount_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                if (status != SWO_SUCCESS) {
                    return status;
                }
                if (output_item->output_data.datum.hasDatum && datum_policy == POLICY_SHOW) {
                    // TODO: Inline datum size is not bounded by protocol; handle large values more robustly.
                    if (output_item->output_data.datum.type == DATUM_HASH) {
                        char *datum_value_tmp = (char *) app_mem_alloc(MAX_DATUM_HASH_STRING_LENGTH + 2);
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
                        char *datum_value_tmp = (char *) app_mem_alloc(max_len + 2);
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
                    char *refscript_tmp = (char *) app_mem_alloc(MAX_REFERENCE_SCRIPT_STRING_LENGTH + 2);
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
                    status = ui_format_token_groups(
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
    char *fee_tmp = (char *) app_mem_alloc(MAX_ADA_AMOUNT_STRING_LENGTH + 2);
    if (fee_tmp == NULL) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    bool fee_formatted = str_formatAdaAmount(tx->fee, fee_tmp, MAX_ADA_AMOUNT_STRING_LENGTH + 2);
    LEDGER_ASSERT(fee_formatted, "Failed to format fee");
    LEDGER_ASSERT(strlen(fee_tmp) <= MAX_ADA_AMOUNT_STRING_LENGTH, "Fee ui string buffer too short");
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
            char *ttl_tmp = (char *) app_mem_alloc(MAX_VALIDITY_BOUNDARY_STRING_LENGTH + 2);
            if (ttl_tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            bool ttl_formatted = str_formatValidityBoundary(tx->ttl,
                                                            tx->networkId,
                                                            tx->protocolMagic,
                                                            ttl_tmp,
                                                            MAX_VALIDITY_BOUNDARY_STRING_LENGTH + 2);
            LEDGER_ASSERT(ttl_formatted, "Failed to format TTL");
            LEDGER_ASSERT(strlen(ttl_tmp) <= MAX_VALIDITY_BOUNDARY_STRING_LENGTH, "TTL ui string buffer too short");
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
    char *epoch_tmp = (char *) app_mem_alloc(MAX_UINT64_STRING_LENGTH + 2);
    if (epoch_tmp == NULL) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    bool epoch_formatted = format_u64(epoch_tmp, MAX_UINT64_STRING_LENGTH + 2,
                                        certificate_data->retirementEpoch);
    LEDGER_ASSERT(epoch_formatted, "Failed to format retirement epoch");
    LEDGER_ASSERT(strlen(epoch_tmp) <= MAX_UINT64_STRING_LENGTH, "Retirement epoch ui string buffer too short");
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
        char *vrf_hash_tmp = (char *) app_mem_alloc(MAX_BECH32_STRING_LENGTH + 2);
        if (vrf_hash_tmp == NULL) {
            return SWO_INSUFFICIENT_MEMORY;
        }
        bool encoded = format_bech32("vrf_vk",
                            certificate_data->vrfKeyHash,
                            VRF_KEY_HASH_LENGTH,
                            vrf_hash_tmp,
                            MAX_BECH32_STRING_LENGTH + 2);
        LEDGER_ASSERT(encoded, "Unable to format VRF key hash");
        LEDGER_ASSERT(strlen(vrf_hash_tmp) <= MAX_BECH32_STRING_LENGTH, "VRF key hash ui string buffer too short");
        status = ui_pairs_add_static_label(UI_STATIC_LABEL("VRF key hash"), vrf_hash_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
        if (status != SWO_SUCCESS) {
            return status;
        }
    }

    // Display pledge
    char *pledge_tmp = (char *) app_mem_alloc(MAX_ADA_AMOUNT_STRING_LENGTH + 2);
    if (pledge_tmp == NULL) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    bool pledge_formatted = str_formatAdaAmount(
        certificate_data->poolRegistration.pledge,
        pledge_tmp,
        MAX_ADA_AMOUNT_STRING_LENGTH + 2);
    LEDGER_ASSERT(pledge_formatted, "Failed to format pledge");
    LEDGER_ASSERT(strlen(pledge_tmp) <= MAX_ADA_AMOUNT_STRING_LENGTH, "Pledge ui string buffer too short");
    status = ui_pairs_add_static_label(UI_STATIC_LABEL("Pledge"), pledge_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
    if (status != SWO_SUCCESS) {
        return status;
    }

    // Display cost
    char *cost_tmp = (char *) app_mem_alloc(MAX_ADA_AMOUNT_STRING_LENGTH + 2);
    if (cost_tmp == NULL) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    bool cost_formatted = str_formatAdaAmount(
        certificate_data->poolRegistration.cost,
        cost_tmp,
        MAX_ADA_AMOUNT_STRING_LENGTH + 2);
    LEDGER_ASSERT(cost_formatted, "Failed to format cost");
    LEDGER_ASSERT(strlen(cost_tmp) <= MAX_ADA_AMOUNT_STRING_LENGTH, "Cost ui string buffer too short");
    status = ui_pairs_add_static_label(UI_STATIC_LABEL("Cost"), cost_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
    if (status != SWO_SUCCESS) {
        return status;
    }

    // Display profit margin as percentage
    // Similar to old app: convert to percentage (0-10000 basis points)
    char *margin_tmp = (char *) app_mem_alloc(MAX_PROFIT_MARGIN_STRING_LENGTH + 2);
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
        G_context.tx_info.transaction.networkId,
        &certificate_data->poolRegistration.rewardAccount
    );
    LEDGER_ASSERT(reward_policy != POLICY_DENY, "Reward account security policy denied");

    // Display reward account
    if (reward_policy == POLICY_SHOW) {
        status = addRewardAccountUIPairs(
            G_context.tx_info.transaction.networkId,
            &certificate_data->poolRegistration.rewardAccount,
            UI_STATIC_LABEL("Pool reward address")
        );
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

        // Check owner security policy
        security_policy_t owner_policy = policyForSignTxStakePoolRegistrationOwner(
            G_context.tx_info.transaction.txSigningMode,
            owner_cred
        );
        LEDGER_ASSERT(owner_policy != POLICY_DENY, "Pool owner security policy denied");

        if (owner_policy == POLICY_SHOW) {
            status = addRewardAddressFromCredentialUIPairs(
                G_context.tx_info.transaction.networkId,
                owner_cred,
                UI_STATIC_LABEL("Owner reward address")
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
        status = ui_pairs_add_static_label_impl(UI_STATIC_LABEL("Pool owners"), (char *) UI_STATIC_LABEL("None"), false) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
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
                char *relay_index_str = (char *) app_mem_alloc(MAX_RELAY_INDEX_STRING_LENGTH + 2);
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
                            char *ipv4_str = (char *) app_mem_alloc(MAX_IPV4_STR_LENGTH + 2);
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
                            char *ipv6_str = (char *) app_mem_alloc(MAX_IPV6_STR_LENGTH + 2);
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
                            char *port_str = (char *) app_mem_alloc(MAX_UINT64_STRING_LENGTH + 2);
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
                            char *dns_str = (char *) app_mem_alloc(relay->dnsNameSize + 2);
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
                            char *port_str = (char *) app_mem_alloc(MAX_UINT64_STRING_LENGTH + 2);
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
                            char *dns_str = (char *) app_mem_alloc(relay->dnsNameSize + 2);
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
    if (relay_idx == 0) {
        warning_bits_set(&G_context.tx_info.warning_bits,
                            WARNING_BIT_POOL_REGISTRATION_NO_RELAYS);
        status = ui_pairs_add_static_label_impl(UI_STATIC_LABEL("Pool relays"), (char *) UI_STATIC_LABEL("None"), false) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
        if (status != SWO_SUCCESS) {
            return status;
        }
    }

    // Display metadata status with appropriate security policy
    if (certificate_data->poolRegistration.poolMetadataIsNull) {
        security_policy_t no_metadata_policy = policyForSignTxStakePoolRegistrationNoMetadata();
        LEDGER_ASSERT(no_metadata_policy != POLICY_DENY, "No metadata security policy denied");

        if (no_metadata_policy == POLICY_SHOW) {
            status = ui_pairs_add_static_label_impl(UI_STATIC_LABEL("Metadata"), (char *) UI_STATIC_LABEL("none (anonymous pool)"), false) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
            if (status != SWO_SUCCESS) {
                return status;
            }
        }
    } else {
        security_policy_t metadata_policy = policyForSignTxStakePoolRegistrationMetadata();
        LEDGER_ASSERT(metadata_policy != POLICY_DENY, "Metadata security policy denied");

        if (metadata_policy == POLICY_SHOW) {
            size_t url_size = certificate_data->poolRegistration.poolMetadata.urlSize;
            char *metadata_url = (char *) app_mem_alloc(url_size + 2);
            if (metadata_url == NULL) return SWO_INSUFFICIENT_MEMORY;
            memcpy(metadata_url,
                    certificate_data->poolRegistration.poolMetadata.url,
                    url_size);
            metadata_url[url_size] = '\0';
            LEDGER_ASSERT(strlen(metadata_url) <= url_size, "Pool metadata url ui string buffer too short");
            status = ui_pairs_add_static_label(UI_STATIC_LABEL("Pool metadata url"), metadata_url) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
            if (status != SWO_SUCCESS) {
                return status;
            }

            char *metadata_hash_tmp = (char *) app_mem_alloc(MAX_POOL_METADATA_HASH_STRING_LENGTH + 2);
            if (metadata_hash_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;
            int hex_status = bytes_to_lowercase_hex(
                metadata_hash_tmp,
                MAX_POOL_METADATA_HASH_STRING_LENGTH + 2,
                certificate_data->poolRegistration.poolMetadata.hash,
                POOL_METADATA_HASH_LENGTH);
            LEDGER_ASSERT(hex_status == 0, "Pool metadata hash formatting failed");
            LEDGER_ASSERT(strlen(metadata_hash_tmp) <= MAX_POOL_METADATA_HASH_STRING_LENGTH, "Pool metadata hash ui string buffer too short");
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
                char *cert_num_tmp = (char *) app_mem_alloc(MAX_UINT64_STRING_LENGTH + 2);
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
                char *cert_type_tmp = (char *) app_mem_alloc(cert_type_len + 1);
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
                char *withdrawal_num_tmp = (char *) app_mem_alloc(MAX_UINT64_STRING_LENGTH + 2);
                if (withdrawal_num_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                snprintf(withdrawal_num_tmp, MAX_UINT64_STRING_LENGTH + 2, "#%d", withdrawal_num);
                LEDGER_ASSERT(strlen(withdrawal_num_tmp) <= MAX_UINT64_STRING_LENGTH, "Withdrawal number ui string buffer too short");
                int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Withdrawal"), withdrawal_num_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                if (status != SWO_SUCCESS) {
                    return status;
                }

                char *withdrawal_amount_tmp = (char *) app_mem_alloc(MAX_ADA_AMOUNT_STRING_LENGTH + 2);
                if (withdrawal_amount_tmp == NULL) {
                    return SWO_INSUFFICIENT_MEMORY;
                }
                bool withdrawal_amount_formatted =
                    str_formatAdaAmount(withdrawal_item->withdrawal_data.amount,
                                        withdrawal_amount_tmp,
                                        MAX_ADA_AMOUNT_STRING_LENGTH + 2);
                LEDGER_ASSERT(withdrawal_amount_formatted, "Failed to format withdrawal amount");
                LEDGER_ASSERT(strlen(withdrawal_amount_tmp) <= MAX_ADA_AMOUNT_STRING_LENGTH, "Withdrawal amount ui string buffer too short");
                status = ui_pairs_add_static_label(UI_STATIC_LABEL("Amount"), withdrawal_amount_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                if (status != SWO_SUCCESS) {
                    return status;
                }

                status = addRewardAccountFromCredentialUIPairs(
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
            char *hash_tmp = (char *) app_mem_alloc(MAX_TX_HASH_DISPLAY_LENGTH + 2);
            if (hash_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;
            int hex_status = bytes_to_lowercase_hex(hash_tmp, MAX_TX_HASH_DISPLAY_LENGTH + 2, tx->auxDataHash, AUX_DATA_HASH_LENGTH);
            LEDGER_ASSERT(hex_status == 0, "Aux data hash hex formatting failed");
            LEDGER_ASSERT(strlen(hash_tmp) <= MAX_TX_HASH_DISPLAY_LENGTH, "Auxiliary data hash ui string buffer too short");
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
            char *validity_interval_start_tmp = (char *) app_mem_alloc(MAX_VALIDITY_BOUNDARY_STRING_LENGTH + 2);
            if (validity_interval_start_tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            bool vis_formatted = str_formatValidityBoundary(tx->validityIntervalStart,
                                                            tx->networkId,
                                                            tx->protocolMagic,
                                                            validity_interval_start_tmp,
                                                            MAX_VALIDITY_BOUNDARY_STRING_LENGTH + 2);
            LEDGER_ASSERT(vis_formatted, "Failed to format validity interval start");
            LEDGER_ASSERT(strlen(validity_interval_start_tmp) <= MAX_VALIDITY_BOUNDARY_STRING_LENGTH, "Validity interval start ui string buffer too short");
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
            char *summary_tmp = (char *) app_mem_alloc(MAX_MINT_SUMMARY_STRING_LENGTH + 2);
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

                    char *fingerprint_tmp = (char *) app_mem_alloc(MAX_TOKEN_FINGERPRINT_STRING_LENGTH + 2);
                    if (fingerprint_tmp == NULL) {
                        return SWO_INSUFFICIENT_MEMORY;
                    }
                    size_t fingerprint_len = deriveAssetFingerprintBech32(
                        tokenGroup.policyId,
                        sizeof(tokenGroup.policyId),
                        token->assetName,
                        token->assetNameLen,
                        fingerprint_tmp,
                        MAX_TOKEN_FINGERPRINT_STRING_LENGTH + 2);
                    LEDGER_ASSERT(fingerprint_len > 0, "Fingerprint derivation failed");
                    LEDGER_ASSERT(strlen(fingerprint_tmp) <= MAX_TOKEN_FINGERPRINT_STRING_LENGTH, "Fingerprint ui string buffer too short");
                    status = ui_pairs_add_static_label(UI_STATIC_LABEL("Mint fingerprint"), fingerprint_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                    if (status != SWO_SUCCESS) {
                        return status;
                    }

                    char *amount_tmp = (char *) app_mem_alloc(MAX_MINT_AMOUNT_STRING_LENGTH + 2);
                    if (amount_tmp == NULL) {
                        return SWO_INSUFFICIENT_MEMORY;
                    }
                    bool mint_amount_formatted = str_formatTokenAmountMint(&tokenGroup,
                                                                           token->assetName,
                                                                           token->assetNameLen,
                                                                           token->amount,
                                                                           amount_tmp,
                                                                           MAX_MINT_AMOUNT_STRING_LENGTH + 2);
                    LEDGER_ASSERT(mint_amount_formatted, "Failed to format mint amount");
                    LEDGER_ASSERT(strlen(amount_tmp) <= MAX_MINT_AMOUNT_STRING_LENGTH, "Mint amount ui string buffer too short");
                    status = ui_pairs_add_static_label(UI_STATIC_LABEL("Mint amount"), amount_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                    if (status != SWO_SUCCESS) {
                        return status;
                    }

                    // Free token node immediately after UI strings are formatted
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
            char *hash_tmp = (char *) app_mem_alloc(MAX_TX_HASH_DISPLAY_LENGTH + 2);
            if (hash_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;
            int hex_status = bytes_to_lowercase_hex(hash_tmp, MAX_TX_HASH_DISPLAY_LENGTH + 2, tx->scriptDataHash, SCRIPT_DATA_HASH_LENGTH);
            LEDGER_ASSERT(hex_status == 0, "Script data hash hex formatting failed");
            LEDGER_ASSERT(strlen(hash_tmp) <= MAX_TX_HASH_DISPLAY_LENGTH, "Script data hash ui string buffer too short");
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
            char *input_tmp = (char *) app_mem_alloc(MAX_INPUT_DISPLAY_STRING_LENGTH + 2);
            if (input_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;
            bool input_formatted = format_input_with_index(input_tmp, MAX_INPUT_DISPLAY_STRING_LENGTH + 2, &input_item->input_data);
            LEDGER_ASSERT(input_formatted, "Failed to format collateral input");
            LEDGER_ASSERT(strlen(input_tmp) <= MAX_INPUT_DISPLAY_STRING_LENGTH, "Collateral input display ui string buffer too short");
            int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Coll input"), input_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
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
                // Allocate buffer large enough for both hash and path formatting
                char *value_tmp = (char *) app_mem_alloc(MAX(MAX_BECH32_STRING_LENGTH, MAX_BIP44_PATH_STRING_LENGTH) + 2);
                if (value_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;

                switch (item->required_signer_data.type) {
                    case REQUIRED_SIGNER_WITH_HASH: {
                        bool encoded = format_bech32("vkh", item->required_signer_data.keyHash, ADDRESS_KEY_HASH_LENGTH, value_tmp, MAX_BECH32_STRING_LENGTH + 2);
                        LEDGER_ASSERT(encoded, "Unable to format required signer key hash");
                        LEDGER_ASSERT(strlen(value_tmp) <= MAX_BECH32_STRING_LENGTH, "Required signer key hash ui string buffer too short");
                        break;
                    }
                    case REQUIRED_SIGNER_WITH_PATH: {
                        bool formatted = format_bip44_path(&item->required_signer_data.keyPath, value_tmp, MAX_BIP44_PATH_STRING_LENGTH + 2);
                        LEDGER_ASSERT(formatted, "Unable to format required signer path");
                        LEDGER_ASSERT(strlen(value_tmp) <= MAX_BIP44_PATH_STRING_LENGTH, "Required signer path ui string buffer too short");
                        break;
                    }
                    default:
                        LEDGER_ASSERT(false, "Unknown required signer type");
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
        char *collateral_address_tmp = (char *) app_mem_alloc(MAX_HUMAN_ADDRESS_LENGTH + 2);
        if (collateral_address_tmp == NULL) {
            return SWO_INSUFFICIENT_MEMORY;
        }

        bool collateral_address_formatted = false;
        if (collateral_desc.destination.type == DESTINATION_THIRD_PARTY) {
            collateral_address_formatted = format_address_human_readable(
                collateral_desc.destination.address.buffer,
                collateral_desc.destination.address.size,
                collateral_address_tmp,
                MAX_HUMAN_ADDRESS_LENGTH + 2);
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
                    MAX_HUMAN_ADDRESS_LENGTH + 2);
            }
        }
        LEDGER_ASSERT(collateral_address_formatted, "Collateral address formatting failed");
        LEDGER_ASSERT(strlen(collateral_address_tmp) <= MAX_HUMAN_ADDRESS_LENGTH, "Collateral address ui string buffer too short");
        status = ui_pairs_add_static_label(UI_STATIC_LABEL("Collateral address"), collateral_address_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
        if (status != SWO_SUCCESS) {
            return status;
        }

        // For device-owned collateral addresses, show payment and staking details
        if (collateral_desc.destination.type == DESTINATION_DEVICE_OWNED) {
            status = addPaymentInfoUIPair(collateral_desc.destination.params);
            if (status != SWO_SUCCESS) {
                return status;
            }
            status = addStakingInfoUIPair(collateral_desc.destination.params);
            if (status != SWO_SUCCESS) {
                return status;
            }
        }

        if (collateral_ada_policy == POLICY_SHOW) {
            char *amount_tmp = (char *) app_mem_alloc(MAX_ADA_AMOUNT_STRING_LENGTH + 2);
            if (amount_tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            bool amount_formatted = str_formatAdaAmount(collateral_desc.amount,
                                                        amount_tmp,
                                                        MAX_ADA_AMOUNT_STRING_LENGTH + 2);
            LEDGER_ASSERT(amount_formatted, "Failed to format collateral amount");
            LEDGER_ASSERT(strlen(amount_tmp) <= MAX_ADA_AMOUNT_STRING_LENGTH, "Collateral amount ui string buffer too short");
            status = ui_pairs_add_static_label(UI_STATIC_LABEL("Collateral amount"), amount_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
            if (status != SWO_SUCCESS) {
                return status;
            }
        }

        if (collateral_confirm_policy == POLICY_SHOW) {
            warning_bits_set(&G_context.tx_info.warning_bits, WARNING_BIT_COLLATERAL_OUTPUT_WARNING);
        }
    }

    status = ui_format_token_groups(
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
        char *amount_tmp = (char *) app_mem_alloc(MAX_ADA_AMOUNT_STRING_LENGTH + 2);
        if (amount_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;
        bool formatted = str_formatAdaAmount(tx->totalCollateral, amount_tmp, MAX_ADA_AMOUNT_STRING_LENGTH + 2);
        LEDGER_ASSERT(formatted, "Failed to format total collateral");
        LEDGER_ASSERT(strlen(amount_tmp) <= MAX_ADA_AMOUNT_STRING_LENGTH, "Total collateral ui string buffer too short");
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
            char *input_tmp = (char *) app_mem_alloc(MAX_INPUT_DISPLAY_STRING_LENGTH + 2);
            if (input_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;
            bool input_formatted = format_input_with_index(input_tmp, MAX_INPUT_DISPLAY_STRING_LENGTH + 2, &input_item->input_data);
            LEDGER_ASSERT(input_formatted, "Failed to format reference input");
            LEDGER_ASSERT(strlen(input_tmp) <= MAX_INPUT_DISPLAY_STRING_LENGTH, "Reference input display ui string buffer too short");
            int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Ref input"), input_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
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
                    char *gov_action_hash_tmp = (char *) app_mem_alloc(MAX_TX_HASH_DISPLAY_LENGTH + 2);
                    if (gov_action_hash_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;

                    int hex_status = bytes_to_lowercase_hex(gov_action_hash_tmp, MAX_TX_HASH_DISPLAY_LENGTH + 2, vote_item->vote_data.govActionId.txHash, TX_HASH_LENGTH);
                    LEDGER_ASSERT(hex_status == 0, "Gov action hash hex formatting failed");
                    LEDGER_ASSERT(strlen(gov_action_hash_tmp) <= MAX_TX_HASH_DISPLAY_LENGTH, "Gov action hash ui string buffer too short");

                    status = ui_pairs_add_static_label(UI_STATIC_LABEL("Gov action tx hash"), gov_action_hash_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
                    if (status != SWO_SUCCESS) return status;

                    // Gov Action Index
                    char *gov_action_index_tmp = (char *) app_mem_alloc(MAX_UINT64_STRING_LENGTH + 2);
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
                    char *vote_str_tmp = (char *) app_mem_alloc(MAX_VOTE_OPTION_LENGTH + 2);
                    if (vote_str_tmp == NULL) return SWO_INSUFFICIENT_MEMORY;
                    strncpy(vote_str_tmp, vote_str, MAX_VOTE_OPTION_LENGTH + 2);
                    LEDGER_ASSERT(strlen(vote_str_tmp) <= MAX_VOTE_OPTION_LENGTH, "Vote option ui string buffer too short");
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
            char *tmp = (char *) app_mem_alloc(MAX_ADA_AMOUNT_STRING_LENGTH + 2);
            if (tmp == NULL) return SWO_INSUFFICIENT_MEMORY;
            bool formatted = str_formatAdaAmount(tx->treasury, tmp, MAX_ADA_AMOUNT_STRING_LENGTH + 2);
            LEDGER_ASSERT(formatted, "Failed to format treasury");
            LEDGER_ASSERT(strlen(tmp) <= MAX_ADA_AMOUNT_STRING_LENGTH, "Treasury ui string buffer too short");
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
            char *tmp = (char *) app_mem_alloc(MAX_ADA_AMOUNT_STRING_LENGTH + 2);
            if (tmp == NULL) return SWO_INSUFFICIENT_MEMORY;
            bool formatted = str_formatAdaAmount(tx->donation, tmp, MAX_ADA_AMOUNT_STRING_LENGTH + 2);
            LEDGER_ASSERT(formatted, "Failed to format donation");
            LEDGER_ASSERT(strlen(tmp) <= MAX_ADA_AMOUNT_STRING_LENGTH, "Donation ui string buffer too short");
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

    char *hash_tmp = (char *) app_mem_alloc(MAX_TX_HASH_DISPLAY_LENGTH + 2);
    if (hash_tmp == NULL) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    int hex_status = bytes_to_lowercase_hex(hash_tmp,
                                            MAX_TX_HASH_DISPLAY_LENGTH + 2,
                                            G_context.tx_info.tx_hash,
                                            sizeof(G_context.tx_info.tx_hash));
    LEDGER_ASSERT(hex_status == 0, "Tx hash hex formatting failed");
    LEDGER_ASSERT(strlen(hash_tmp) <= MAX_TX_HASH_DISPLAY_LENGTH, "Tx hash ui string buffer too short");
    int status = ui_pairs_add_static_label(UI_STATIC_LABEL("Transaction hash"), hash_tmp) ? SWO_SUCCESS : SWO_INSUFFICIENT_MEMORY;
    if (status != SWO_SUCCESS) {
        return status;
    }
    return SWO_SUCCESS;
}

static int add_ui_strings_and_free_parsed_data(void) {
    LEDGER_ASSERT(G_context.state.tx_state == TX_STATE_HASHED, "String formatting invoked too early");
    transaction_t *tx = &G_context.tx_info.transaction;
    int status;

    TRACE("UI formatting starting");

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

    TRACE("UI formatting complete");
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
    return ui_build_warnings(G_context.tx_info.warning_bits);
}

int ui_prepare_transaction_review(void) {
    if (G_context.req_type != REQUEST_SIGN_TRANSACTION) {
        return send_swo_and_reset(SWO_BAD_STATE);
    }
    LEDGER_ASSERT(G_context.state.tx_state == TX_STATE_HASHED, "UI prep called too early");
    uint16_t pair_count = G_context.tx_info.planned_ui_pairs;

    // pair_count should never be 0 - at minimum we display fee
    LEDGER_ASSERT(pair_count > 0, "UI pair count is zero - at minimum fee must be displayed");

    // If pair count exceeds NBGL capability, reject the transaction
    if (pair_count > UINT8_MAX) {
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
