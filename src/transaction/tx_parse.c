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
#include "buffer.h"
#include "memory/mem.h"

#include "cardano_swo.h"
#include "utils/cardano_os_utils.h"
#include "utils/buffer_utils.h"
#include "tx_parse.h"
#include "tx_parse_certificates.h"
#include "tx_parse_outputs.h"
#include "transaction/tx.h"
#include "utils.h"
#include "utils/assert.h"
#include "utils/textUtils.h"
#include "transaction/tx_constants.h"
#include "tx_output_types.h"
#include "globals.h"

static parser_status_e parse_input_item(buffer_t *buf, s_flist_node **list_head, parser_status_e error_on_failure);
static parser_status_e parse_tx_inputs(buffer_t *buf, transaction_t *tx);
static parser_status_e parse_tx_outputs(buffer_t *buf, transaction_t *tx);
static parser_status_e parse_tx_mint_groups(buffer_t *buf, transaction_t *tx);
static parser_status_e parse_tx_certificates(buffer_t *buf, transaction_t *tx);
static parser_status_e parse_tx_withdrawals(buffer_t *buf, transaction_t *tx);
static parser_status_e parse_tx_collateral_inputs(buffer_t *buf, transaction_t *tx);
static parser_status_e parse_tx_required_signers(buffer_t *buf, transaction_t *tx);
static parser_status_e parse_tx_collateral_output(buffer_t *buf, transaction_t *tx);

static uint16_t _map_parser_status_to_swo(parser_status_e status) {
    switch (status) {
        // Transaction body CBOR key order:
        case INPUTS_PARSING_ERROR:              // key 0
        case INPUTS_COUNT_PARSING_ERROR:
            return SWO_TX_PARSING_FAIL_INPUTS;
        case OUTPUTS_PARSING_ERROR:             // key 1
        case OUTPUTS_COUNT_PARSING_ERROR:
        case OUTPUT_DESTINATION_TYPE_ERROR:
        case OUTPUT_ADDRESS_SIZE_ERROR:
            return SWO_TX_PARSING_FAIL_OUTPUTS;
        case FEE_PARSING_ERROR:                 // key 2
            return SWO_TX_PARSING_FAIL_FEE;
        case TTL_PARSING_ERROR:                 // key 3
            return SWO_TX_PARSING_FAIL_TTL;
        case CERTIFICATES_PARSING_ERROR:        // key 4
            return SWO_TX_PARSING_FAIL_CERTIFICATES;
        case WITHDRAWALS_PARSING_ERROR:         // key 5
            return SWO_TX_PARSING_FAIL_WITHDRAWALS;
        // key 7 is update (not supported)
        case VALIDITY_INTERVAL_START_PARSING_ERROR:  // key 8
            return SWO_TX_PARSING_FAIL_VALIDITY_INTERVAL_START;
        case MINT_PARSING_ERROR:                // key 9
            return SWO_TX_PARSING_FAIL_MINT;
        // key 11 is script data hash (parsed inline)
        // key 13 is required signers (parsed inline)
        // key 14 is network id (parsed inline)
        // key 15 is collateral return (parsed inline)
        // key 16 is total collateral (parsed inline)
        // key 17 is reference inputs (parsed inline)
        // key 19 is voting procedures (not supported)
        // key 20 is proposal procedures (not supported)
        // key 21 is treasury donation (not supported)
        case TX_SIZE_TOO_LARGE_ERROR:
            return SWO_INVALID_TX_LENGTH;
        case TX_BUFFER_NOT_FULLY_CONSUMED_ERROR:
            return SWO_TX_PARSING_FAIL_BUFFER_NOT_FULLY_CONSUMED;
        case OUT_OF_MEMORY_ERROR:
            return SWO_INSUFFICIENT_MEMORY;
        default:
            return SWO_TX_PARSING_FAIL;
    }
}

parser_status_e parse_tx(buffer_t *buf, transaction_t *tx) {
    LEDGER_ASSERT(buf != NULL, "NULL buf");
    LEDGER_ASSERT(buf->ptr != NULL, "NULL buffer ptr");
    LEDGER_ASSERT(tx != NULL, "NULL tx");

    if (buf->size > TX_BUFFER_SIZE) {
        return TX_SIZE_TOO_LARGE_ERROR;
    }

    // Initialize lists
    tx->inputs = NULL;
    tx->outputs = NULL;
    tx->withdrawals = NULL;
    tx->certificates = NULL;
    tx->mint_asset_groups = NULL;

    parser_status_e status;

    // key 0: inputs
    status = parse_tx_inputs(buf, tx);
    if (status != PARSING_OK) {
        return status;
    }

    // key 1: outputs
    status = parse_tx_outputs(buf, tx);
    if (status != PARSING_OK) {
        return status;
    }

    // key 2: fee
    if (!buffer_read_u64(buf, &tx->fee, BE)) {
        return FEE_PARSING_ERROR;
    }

    // key 3: ttl (optional)
    if (tx->includeTtl) {
        if (!buffer_read_u64(buf, &tx->ttl, BE)) {
            return TTL_PARSING_ERROR;
        }
    }

    // key 4: certificates
    TRACE("About to parse %u certificates", tx->num_certificates);
    status = parse_tx_certificates(buf, tx);
    if (status != PARSING_OK) {
        TRACE("Certificate parsing failed with status=%d", status);
        return status;
    }
    TRACE("Successfully parsed all certificates");

    // key 5: withdrawals
    status = parse_tx_withdrawals(buf, tx);
    if (status != PARSING_OK) {
        return status;
    }

    // key 8: validity_interval_start
    if (tx->includeValidityIntervalStart) {
        if (!buffer_read_u64(buf, &tx->validityIntervalStart, BE)) {
            return VALIDITY_INTERVAL_START_PARSING_ERROR;
        }
    }

    // key 9: mint
    status = parse_tx_mint_groups(buf, tx);
    if (status != PARSING_OK) {
        return status;
    }

    // key 11: script data hash
    if (tx->includeScriptDataHash) {
        if (!buffer_read_bytes(buf, tx->scriptDataHash, SCRIPT_DATA_HASH_LENGTH)) {
            return SCRIPT_DATA_HASH_PARSING_ERROR;
        }
    }

    // key 13: collateral inputs
    if (tx->num_collateral_inputs > 0) {
        status = parse_tx_collateral_inputs(buf, tx);
        if (status != PARSING_OK) {
            return status;
        }
    }

    // key 14: required signers
    if (tx->num_required_signers > 0) {
        status = parse_tx_required_signers(buf, tx);
        if (status != PARSING_OK) {
            return status;
        }
    }

    // key 15: network ID - nothing to parse, just a flag (already in init APDU)

    // key 16: collateral output
    if (tx->includeCollateralOutput) {
        status = parse_tx_collateral_output(buf, tx);
        if (status != PARSING_OK) {
            return status;
        }
    }

    // key 17: total collateral
    if (tx->includeTotalCollateral) {
        if (!buffer_read_u64(buf, &tx->totalCollateral, BE)) {
            return TOTAL_COLLATERAL_PARSING_ERROR;
        }
    }

    // key 18: reference inputs (parsed same as regular inputs)
    if (tx->num_reference_inputs > 0) {
        for (uint16_t i = 0; i < tx->num_reference_inputs; i++) {
            status = parse_input_item(buf, &tx->reference_inputs, REFERENCE_INPUTS_PARSING_ERROR);
            if (status != PARSING_OK) {
                return status;
            }
        }
    }

    if (buffer_can_read(buf, 1)) {
        TRACE("TX parsing: buffer not fully consumed");
        return TX_BUFFER_NOT_FULLY_CONSUMED_ERROR;
    }
    return PARSING_OK;
}


int tx_handle_parse_error(parser_status_e status) {
    LEDGER_ASSERT(status != PARSING_OK, "tx_handle_parse_error received PARSING_OK");
    tx_context_cleanup();
    uint16_t swo = _map_parser_status_to_swo(status);
    TRACE("tx_handle_parse_error status=%d swo=0x%04x", status, swo);
    return send_error_and_reset(swo);
}

// Helper function to parse a single input (reused for inputs, collateral inputs, reference inputs)
// error_on_failure: error code to return if parsing fails (e.g., INPUTS_PARSING_ERROR, COLLATERAL_INPUTS_PARSING_ERROR)
static parser_status_e parse_input_item(buffer_t *buf, s_flist_node **list_head, parser_status_e error_on_failure) {
    tx_input_list_item_t *item = (tx_input_list_item_t *) app_mem_alloc(sizeof(tx_input_list_item_t));
    if (item == NULL) {
        return OUT_OF_MEMORY_ERROR;
    }

    // Store pointer to tx hash in raw buffer instead of copying
    uint8_t *hash_ptr = NULL;
    if (!buffer_read_bytes_ptr(buf, &hash_ptr, TX_HASH_LENGTH)) {
        return error_on_failure;
    }
    ASSERT(hash_ptr != NULL);
    item->input_data.txHash = hash_ptr;

    if (!buffer_read_u32(buf, &item->input_data.index, BE)) {
        return error_on_failure;
    }

    item->node.next = NULL;
    flist_push_back(list_head, (s_flist_node *) item);
    return PARSING_OK;
}

static parser_status_e parse_tx_inputs(buffer_t *buf, transaction_t *tx) {
    for (uint16_t i = 0; i < tx->num_inputs; i++) {
        parser_status_e status = parse_input_item(buf, &tx->inputs, INPUTS_PARSING_ERROR);
        if (status != PARSING_OK) {
            return status;
        }
    }
    return PARSING_OK;
}

static parser_status_e parse_tx_outputs(buffer_t *buf, transaction_t *tx) {
    for (uint16_t i = 0; i < tx->num_outputs; i++) {
        uint16_t output_len;
        if (!buffer_read_u16(buf, &output_len, BE)) {
            return OUTPUTS_PARSING_ERROR;
        }
        TRACE("Deserialize: Output %u: length=%u, buffer offset before parse=%u",
              i, output_len, buf->offset);

        // Create sub-buffer for this output with exact length
        if (!buffer_can_read(buf, output_len)) {
            return OUTPUTS_PARSING_ERROR;
        }
        buffer_t output_buf = {
            .ptr = buf->ptr + buf->offset,  // Current position in main buffer
            .size = output_len,              // Length of this output
            .offset = 0                      // Start parsing from beginning of sub-buffer
        };

        tx_output_list_item_t *item = (tx_output_list_item_t *) app_mem_alloc(sizeof(tx_output_list_item_t));
        if (item == NULL) {
            return OUT_OF_MEMORY_ERROR;
        }

        // Parse destination (third-party address or device-owned address params)
        parser_status_e status = parse_output_destination(&output_buf,
                                                          &item->output_data.destination,
                                                          tx->networkId);
        if (status != PARSING_OK) {
            return status;
        }
        TRACE("Deserialize: Output %u destination parsed", i);

        if (!buffer_read_u64(&output_buf, &item->output_data.adaAmount, BE)) {
            return OUTPUTS_PARSING_ERROR;
        }

        // Parse output format (ARRAY_LEGACY or MAP_BABBAGE)
        status = parse_output_format(&output_buf, &item->output_data.format);
        if (status != PARSING_OK) {
            return status;
        }
        TRACE("Deserialize: Output %u format parsed", i);

        if (!buffer_read_u16(&output_buf, &item->output_data.numAssetGroups, BE)) {
            return OUTPUTS_PARSING_ERROR;
        }
        TRACE("Deserialize: Output %u: %u asset groups", i, item->output_data.numAssetGroups);

        if (item->output_data.numAssetGroups > 0) {
            item->output_data.assetGroups =
                (asset_group_t *) app_mem_alloc(item->output_data.numAssetGroups * sizeof(asset_group_t));
            if (item->output_data.assetGroups == NULL) {
                return OUT_OF_MEMORY_ERROR;
            }

            for (uint16_t ag = 0; ag < item->output_data.numAssetGroups; ag++) {
                asset_group_t *group = &item->output_data.assetGroups[ag];

                // Store pointer to policy ID in raw buffer instead of copying
                uint8_t *policy_ptr = NULL;
                if (!buffer_read_bytes_ptr(&output_buf, &policy_ptr, MINTING_POLICY_ID_LENGTH)) {
                    return OUTPUTS_PARSING_ERROR;
                }
                ASSERT(policy_ptr != NULL);
                group->policyId = policy_ptr;
                TRACE("Deserialize: Asset group %u: policy ID read", ag);

                if (!buffer_read_u16(&output_buf, &group->numTokens, BE)) {
                    return OUTPUTS_PARSING_ERROR;
                }
                TRACE("Deserialize: Asset group %u: %u tokens", ag, group->numTokens);

                group->tokens = (output_token_t *) app_mem_alloc(group->numTokens * sizeof(output_token_t));
                if (group->tokens == NULL) {
                    return OUT_OF_MEMORY_ERROR;
                }

                for (uint16_t tk = 0; tk < group->numTokens; tk++) {
                    output_token_t *token = &group->tokens[tk];
                    if (!buffer_read_u8(&output_buf, &token->assetNameLen)) {
                        return OUTPUTS_PARSING_ERROR;
                    }
                    if (token->assetNameLen > MAX_ASSET_NAME_LENGTH) {
                        return OUTPUTS_PARSING_ERROR;
                    }

                    // Store pointer to asset name in raw buffer instead of copying
                    // Note: assetNameLen can be 0 for empty asset names, which is valid
                    uint8_t *name_ptr = NULL;
                    if (!buffer_read_bytes_ptr(&output_buf, &name_ptr, token->assetNameLen)) {
                        return OUTPUTS_PARSING_ERROR;
                    }
                    ASSERT(name_ptr != NULL);
                    token->assetName = name_ptr;

                    if (!buffer_read_u64(&output_buf, &token->amount, BE)) {
                        return OUTPUTS_PARSING_ERROR;
                    }

                    TRACE("Deserialize: Token %u: name_len=%u, amount=",
                          tk, token->assetNameLen);
                    TRACE_UINT64(token->amount);
                }
            }
        } else {
            item->output_data.assetGroups = NULL;
        }

        // Parse datum (hash, inline, or none)
        status = parse_output_datum(&output_buf, &item->output_data.datum);
        if (status != PARSING_OK) {
            return status;
        }
        TRACE("Deserialize: Output %u datum parsed", i);

        // Parse reference script (if present)
        status = parse_output_ref_script(&output_buf,
                                        &item->output_data.refScript,
                                        &item->output_data.hasRefScript);
        if (status != PARSING_OK) {
            return status;
        }
        TRACE("Deserialize: Output %u reference script parsed", i);

        // Verify output_buf was fully consumed (nothing more, nothing less)
        if (buffer_can_read(&output_buf, 1)) {
            TRACE("Deserialize: Output %u buffer not fully consumed: offset=%u, size=%u",
                  i, output_buf.offset, output_buf.size);
            return OUTPUTS_PARSING_ERROR;
        }
        TRACE("Deserialize: Output %u fully consumed, advancing main buffer by %u bytes",
              i, output_len);

        // Advance main buffer past this output
        if (!buffer_seek_cur(buf, output_len)) {
            return OUTPUTS_PARSING_ERROR;
        }
        TRACE("Deserialize: Output %u complete, buffer offset now=%u", i, buf->offset);

        item->node.next = NULL;
        flist_push_back(&tx->outputs, (s_flist_node *) item);
    }
    return PARSING_OK;
}

static parser_status_e parse_tx_mint_groups(buffer_t *buf, transaction_t *tx) {
    for (uint16_t ag = 0; ag < tx->num_mint_asset_groups; ag++) {
        mint_asset_group_list_item_t *item = (mint_asset_group_list_item_t *) app_mem_alloc(sizeof(mint_asset_group_list_item_t));
        if (item == NULL) {
            return OUT_OF_MEMORY_ERROR;
        }

        // Store pointer to policy ID in raw buffer instead of copying
        uint8_t *policy_ptr = NULL;
        if (!buffer_read_bytes_ptr(buf, &policy_ptr, MINTING_POLICY_ID_LENGTH)) {
            return MINT_PARSING_ERROR;
        }
        ASSERT(policy_ptr != NULL);
        item->asset_group.policyId = policy_ptr;
        TRACE("Deserialize: Mint asset group %u: policy ID read", ag);

        if (!buffer_read_u16(buf, &item->asset_group.numTokens, BE)) {
            return MINT_PARSING_ERROR;
        }
        TRACE("Deserialize: Mint asset group %u: %u tokens", ag, item->asset_group.numTokens);

        item->asset_group.tokens = (mint_token_t *) app_mem_alloc(item->asset_group.numTokens * sizeof(mint_token_t));
        if (item->asset_group.tokens == NULL) {
            return OUT_OF_MEMORY_ERROR;
        }

        for (uint16_t tk = 0; tk < item->asset_group.numTokens; tk++) {
            mint_token_t *token = &item->asset_group.tokens[tk];
            if (!buffer_read_u8(buf, &token->assetNameLen)) {
                return MINT_PARSING_ERROR;
            }
            if (token->assetNameLen > MAX_MINT_ASSET_NAME_LENGTH) {
                return MINT_PARSING_ERROR;
            }

            // Store pointer to asset name in raw buffer instead of copying
            // Note: assetNameLen can be 0 for empty asset names, which is valid
            uint8_t *name_ptr = NULL;
            if (!buffer_read_bytes_ptr(buf, &name_ptr, token->assetNameLen)) {
                return MINT_PARSING_ERROR;
            }
            ASSERT(name_ptr != NULL);
            token->assetName = name_ptr;

            if (!buffer_read_int64(buf, &token->amount, BE)) {
                return MINT_PARSING_ERROR;
            }

            TRACE("Deserialize: Mint token %u: name_len=%u, amount=", tk, token->assetNameLen);
            TRACE_INT64(token->amount);
        }

        item->node.next = NULL;
        flist_push_back(&tx->mint_asset_groups, (s_flist_node *) item);
    }
    return PARSING_OK;
}

/// Parse certificate data structure supporting multiple certificate types
static parser_status_e parse_tx_certificates(buffer_t *buf, transaction_t *tx) {
    TRACE(">>>>> parse_tx_certificates: num_certificates=%u buf->offset=%u buf->size=%u",
          tx->num_certificates, buf->offset, buf->size);
    for (uint16_t i = 0; i < tx->num_certificates; i++) {
        TRACE(">>>>> Allocating memory for certificate %u", i);
        tx_certificate_list_item_t *item = (tx_certificate_list_item_t *) app_mem_alloc(sizeof(tx_certificate_list_item_t));
        if (item == NULL) {
            TRACE(">>>>> OUT OF MEMORY");
            return OUT_OF_MEMORY_ERROR;
        }

        // Read certificate type
        uint8_t cert_type_wire;
        TRACE(">>>>> About to read certificate type byte at offset %u", buf->offset);
        if (!buffer_read_u8(buf, &cert_type_wire)) {
            TRACE(">>>>> FAILED TO READ CERTIFICATE TYPE BYTE");
            return CERTIFICATES_PARSING_ERROR;
        }
        certificate_type_t cert_type = (certificate_type_t) cert_type_wire;
        TRACE("Deserialize: Certificate %u type=%u", i, cert_type_wire);

        // Parse certificate data based on type
        parser_status_e status = PARSING_OK;
        switch (cert_type) {
            case CERTIFICATE_STAKE_REGISTRATION:
            case CERTIFICATE_STAKE_DEREGISTRATION:
                status = parse_certificate_stake_registration_deregistration(buf, cert_type, &item->certificate_data);
                break;

            case CERTIFICATE_STAKE_DELEGATION:
                status = parse_certificate_stake_delegation(buf, &item->certificate_data);
                break;

            case CERTIFICATE_STAKE_REGISTRATION_CONWAY:
            case CERTIFICATE_STAKE_DEREGISTRATION_CONWAY:
                status = parse_certificate_stake_registration_deregistration_conway(buf, cert_type, &item->certificate_data);
                break;

            case CERTIFICATE_STAKE_POOL_RETIREMENT:
                status = parse_certificate_stake_pool_retirement(buf, &item->certificate_data);
                break;

            case CERTIFICATE_VOTE_DELEGATION:
                status = parse_certificate_vote_delegation(buf, &item->certificate_data);
                break;

            case CERTIFICATE_AUTHORIZE_COMMITTEE_HOT:
                status = parse_certificate_authorize_committee_hot(buf, &item->certificate_data);
                break;

            case CERTIFICATE_RESIGN_COMMITTEE_COLD:
                status = parse_certificate_resign_committee_cold(buf, &item->certificate_data);
                break;

            case CERTIFICATE_DREP_REGISTRATION:
                status = parse_certificate_drep_registration(buf, &item->certificate_data);
                break;

            case CERTIFICATE_DREP_DEREGISTRATION:
                status = parse_certificate_drep_deregistration(buf, &item->certificate_data);
                break;

            case CERTIFICATE_DREP_UPDATE:
                status = parse_certificate_drep_update(buf, &item->certificate_data);
                break;

        default:
            // Pool registration not in scope for this implementation
            status = CERTIFICATES_PARSING_ERROR;
            break;
        }

        if (status != PARSING_OK) {
            TRACE("Certificate parse failure: type=%u status=%d", cert_type_wire, status);
            return status;
        }

        item->node.next = NULL;
        flist_push_back(&tx->certificates, (s_flist_node *) item);
    }
    return PARSING_OK;
}

static parser_status_e parse_tx_withdrawals(buffer_t *buf, transaction_t *tx) {
    for (uint16_t i = 0; i < tx->num_withdrawals; i++) {
        tx_withdrawal_list_item_t *item = (tx_withdrawal_list_item_t *) app_mem_alloc(sizeof(tx_withdrawal_list_item_t));
        if (item == NULL) {
            return OUT_OF_MEMORY_ERROR;
        }

        if (!buffer_read_u64(buf, &item->withdrawal_data.amount, BE)) {
            return WITHDRAWALS_PARSING_ERROR;
        }

        parser_status_e status = parse_stake_credential(buf, &item->withdrawal_data.stakeCredential);
        if (status != PARSING_OK) {
            TRACE("Withdrawal %u credential parsing failed: status=%d", i, status);
            return status;
        }

        TRACE("Deserialize: Withdrawal %u, type=%u", i, item->withdrawal_data.stakeCredential.type);

        item->node.next = NULL;
        flist_push_back(&tx->withdrawals, (s_flist_node *) item);
    }
    return PARSING_OK;
}

/// Clean up dynamically allocated memory in transaction outputs (including list items)
void transaction_free_outputs(transaction_t *tx) {
    LEDGER_ASSERT(tx != NULL, "NULL tx");

    s_flist_node *output_node = tx->outputs;
    while (output_node != NULL) {
        tx_output_list_item_t *item = (tx_output_list_item_t *) output_node;
        s_flist_node *next = output_node->next;

        // Free asset groups and their tokens
        if (item->output_data.assetGroups != NULL) {
            for (uint16_t i = 0; i < item->output_data.numAssetGroups; i++) {
                if (item->output_data.assetGroups[i].tokens != NULL) {
                    app_mem_free(item->output_data.assetGroups[i].tokens);
                }
            }
            app_mem_free(item->output_data.assetGroups);
        }

        // Note: inline datum and reference script data are pointers into the raw_tx buffer,
        // not separately allocated, so they do not need to be freed

        // Free the list item itself
        app_mem_free(output_node);
        output_node = next;
    }
    tx->outputs = NULL;
}

/// Clean up dynamically allocated memory in transaction certificates (including list items)
void transaction_free_certificates(transaction_t *tx) {
    LEDGER_ASSERT(tx != NULL, "NULL tx");

    s_flist_node *certificate_node = tx->certificates;
    while (certificate_node != NULL) {
        // Certificate items don't have additional allocated memory
        // (credential data is stored inline in the union)
        s_flist_node *next = certificate_node->next;
        app_mem_free(certificate_node);
        certificate_node = next;
    }
    tx->certificates = NULL;
}

void transaction_free_withdrawals(transaction_t *tx) {
    LEDGER_ASSERT(tx != NULL, "NULL tx");

    s_flist_node *withdrawal_node = tx->withdrawals;
    while (withdrawal_node != NULL) {
        // Withdrawal items don't have additional allocated memory
        // (credential data is stored inline in the union)
        s_flist_node *next = withdrawal_node->next;
        app_mem_free(withdrawal_node);
        withdrawal_node = next;
    }
    tx->withdrawals = NULL;
}

/// Clean up dynamically allocated memory in transaction mint (including list items)
void transaction_free_mint(transaction_t *tx) {
    LEDGER_ASSERT(tx != NULL, "NULL tx");

    s_flist_node *mint_node = tx->mint_asset_groups;
    while (mint_node != NULL) {
        mint_asset_group_list_item_t *item = (mint_asset_group_list_item_t *) mint_node;
        s_flist_node *next = mint_node->next;

        // Free tokens array
        if (item->asset_group.tokens != NULL) {
            app_mem_free(item->asset_group.tokens);
        }

        // Free the list item itself
        app_mem_free(mint_node);
        mint_node = next;
    }
    tx->mint_asset_groups = NULL;
}

/// Clean up collateral inputs (same structure as regular inputs)
void transaction_free_collateral_inputs(transaction_t *tx) {
    LEDGER_ASSERT(tx != NULL, "NULL tx");

    s_flist_node *input_node = tx->collateral_inputs;
    while (input_node != NULL) {
        s_flist_node *next = input_node->next;
        app_mem_free(input_node);
        input_node = next;
    }
    tx->collateral_inputs = NULL;
}

/// Clean up required signers
void transaction_free_required_signers(transaction_t *tx) {
    LEDGER_ASSERT(tx != NULL, "NULL tx");

    s_flist_node *signer_node = tx->required_signers;
    while (signer_node != NULL) {
        s_flist_node *next = signer_node->next;
        app_mem_free(signer_node);
        signer_node = next;
    }
    tx->required_signers = NULL;
}

/// Clean up reference inputs (same structure as regular inputs)
void transaction_free_reference_inputs(transaction_t *tx) {
    LEDGER_ASSERT(tx != NULL, "NULL tx");

    s_flist_node *input_node = tx->reference_inputs;
    while (input_node != NULL) {
        s_flist_node *next = input_node->next;
        app_mem_free(input_node);
        input_node = next;
    }
    tx->reference_inputs = NULL;
}

/**
 * Cleanup transaction lists by freeing all allocated memory
 */
void tx_context_cleanup(void) {
    transaction_t *tx = &G_context.tx_info.transaction;

    // Free in CBOR key order (matches transaction_body CDDL)
    // key 0: inputs
    s_flist_node *input_node = tx->inputs;
    while (input_node != NULL) {
        s_flist_node *next = input_node->next;
        app_mem_free(input_node);
        input_node = next;
    }
    tx->inputs = NULL;

    // key 1: outputs
    transaction_free_outputs(tx);

    // key 4: certificates
    transaction_free_certificates(tx);

    // key 5: withdrawals
    transaction_free_withdrawals(tx);

    // key 9: mint
    transaction_free_mint(tx);

    // key 13: collateral inputs
    transaction_free_collateral_inputs(tx);

    // key 14: required signers
    transaction_free_required_signers(tx);

    // key 18: reference inputs
    transaction_free_reference_inputs(tx);

    // Free raw tx buffer
    if (G_context.tx_info.raw_tx != NULL) {
        app_mem_free(G_context.tx_info.raw_tx);
        G_context.tx_info.raw_tx = NULL;
    }
    G_context.tx_info.planned_ui_pairs = 0;
}

// ================== Parsing functions for elements 13-18 ==================

static parser_status_e parse_tx_collateral_inputs(buffer_t *buf, transaction_t *tx) {
    for (uint16_t i = 0; i < tx->num_collateral_inputs; i++) {
        parser_status_e status = parse_input_item(buf, &tx->collateral_inputs, COLLATERAL_INPUTS_PARSING_ERROR);
        if (status != PARSING_OK) {
            return status;
        }
    }
    return PARSING_OK;
}

static parser_status_e parse_tx_required_signers(buffer_t *buf, transaction_t *tx) {
    for (uint16_t i = 0; i < tx->num_required_signers; i++) {
        tx_required_signer_list_item_t *item =
            (tx_required_signer_list_item_t *) app_mem_alloc(sizeof(tx_required_signer_list_item_t));
        if (item == NULL) {
            return OUT_OF_MEMORY_ERROR;
        }

        // Read signer type (1 byte)
        uint8_t type;
        if (!buffer_read_u8(buf, &type)) {
            return REQUIRED_SIGNERS_PARSING_ERROR;
        }
        item->required_signer_data.type = (required_signer_type_t) type;

        // Read path or hash based on type
        switch (item->required_signer_data.type) {
            case REQUIRED_SIGNER_WITH_PATH:
                // Parse BIP44 path
                if (!buffer_read_bip44_path(buf, &item->required_signer_data.keyPath)) {
                    return REQUIRED_SIGNERS_PARSING_ERROR;
                }
                break;
            case REQUIRED_SIGNER_WITH_HASH:
                // Read 28-byte key hash
                if (!buffer_read_bytes(buf, item->required_signer_data.keyHash, ADDRESS_KEY_HASH_LENGTH)) {
                    return REQUIRED_SIGNERS_PARSING_ERROR;
                }
                break;
            default:
                return REQUIRED_SIGNERS_PARSING_ERROR;
        }

        item->node.next = NULL;
        flist_push_back(&tx->required_signers, (s_flist_node *) item);
    }
    return PARSING_OK;
}

// Helper function to parse output structure (reused for regular and collateral outputs)
static parser_status_e parse_output_structure(buffer_t *output_buf,
                                             tx_output_destination_storage_t *destination,
                                             uint64_t *adaAmount,
                                             tx_output_serialization_format_t *format,
                                             uint16_t *numAssetGroups,
                                             asset_group_t **assetGroups,
                                             uint8_t networkId) {
    // Parse destination
    parser_status_e status = parse_output_destination(output_buf, destination, networkId);
    if (status != PARSING_OK) {
        return status;
    }

    // Read ADA amount
    if (!buffer_read_u64(output_buf, adaAmount, BE)) {
        return OUTPUTS_PARSING_ERROR;
    }

    // Parse output format
    status = parse_output_format(output_buf, format);
    if (status != PARSING_OK) {
        return status;
    }

    // Read asset group count
    if (!buffer_read_u16(output_buf, numAssetGroups, BE)) {
        return OUTPUTS_PARSING_ERROR;
    }

    // Parse asset groups
    if (*numAssetGroups > 0) {
        *assetGroups = (asset_group_t *) app_mem_alloc(*numAssetGroups * sizeof(asset_group_t));
        if (*assetGroups == NULL) {
            return OUT_OF_MEMORY_ERROR;
        }

        for (uint16_t ag = 0; ag < *numAssetGroups; ag++) {
            asset_group_t *group = &(*assetGroups)[ag];

            uint8_t *policy_ptr = NULL;
            if (!buffer_read_bytes_ptr(output_buf, &policy_ptr, MINTING_POLICY_ID_LENGTH)) {
                return OUTPUTS_PARSING_ERROR;
            }
            ASSERT(policy_ptr != NULL);
            group->policyId = policy_ptr;

            if (!buffer_read_u16(output_buf, &group->numTokens, BE)) {
                return OUTPUTS_PARSING_ERROR;
            }

            group->tokens = (output_token_t *) app_mem_alloc(group->numTokens * sizeof(output_token_t));
            if (group->tokens == NULL) {
                return OUT_OF_MEMORY_ERROR;
            }

            for (uint16_t tk = 0; tk < group->numTokens; tk++) {
                output_token_t *token = &group->tokens[tk];
                if (!buffer_read_u8(output_buf, &token->assetNameLen)) {
                    return OUTPUTS_PARSING_ERROR;
                }
                if (token->assetNameLen > MAX_ASSET_NAME_LENGTH) {
                    return OUTPUTS_PARSING_ERROR;
                }

                uint8_t *name_ptr = NULL;
                if (!buffer_read_bytes_ptr(output_buf, &name_ptr, token->assetNameLen)) {
                    return OUTPUTS_PARSING_ERROR;
                }
                ASSERT(name_ptr != NULL);
                token->assetName = name_ptr;

                if (!buffer_read_u64(output_buf, &token->amount, BE)) {
                    return OUTPUTS_PARSING_ERROR;
                }
            }
        }
    } else {
        *assetGroups = NULL;
    }

    return PARSING_OK;
}

static parser_status_e parse_tx_collateral_output(buffer_t *buf, transaction_t *tx) {
    uint16_t output_len;
    if (!buffer_read_u16(buf, &output_len, BE)) {
        return COLLATERAL_OUTPUT_PARSING_ERROR;
    }

    if (!buffer_can_read(buf, output_len)) {
        return COLLATERAL_OUTPUT_PARSING_ERROR;
    }

    buffer_t output_buf = {
        .ptr = buf->ptr + buf->offset,
        .size = output_len,
        .offset = 0
    };

    // Reuse output structure parsing
    parser_status_e status = parse_output_structure(&output_buf,
                                                   &tx->collateral_output.destination,
                                                   &tx->collateral_output.adaAmount,
                                                   &tx->collateral_output.format,
                                                   &tx->collateral_output.numAssetGroups,
                                                   &tx->collateral_output.assetGroups,
                                                   tx->networkId);
    if (status != PARSING_OK) {
        return status;
    }

    buf->offset += output_len;
    return PARSING_OK;
}

// Reference inputs parsing is inline in parse_tx() since they use the same format as regular inputs
