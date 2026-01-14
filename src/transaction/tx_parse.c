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
#include "app_context.h"
#include "utils/buffer_utils.h"
#include "utils/cbor.h"
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
static parser_status_e parse_tx_voting_procedures(buffer_t *buf, transaction_t *tx);

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
        case SCRIPT_DATA_HASH_PARSING_ERROR:    // key 11
            return SWO_TX_PARSING_FAIL_SCRIPT_DATA_HASH;
        case COLLATERAL_INPUTS_PARSING_ERROR:   // key 13
            return SWO_TX_PARSING_FAIL_COLLATERAL_INPUTS;
        case REQUIRED_SIGNERS_PARSING_ERROR:    // key 14
            return SWO_TX_PARSING_FAIL_REQUIRED_SIGNERS;
        // key 15: network id - nothing to parse in body
        case COLLATERAL_OUTPUT_PARSING_ERROR:   // key 16
            return SWO_TX_PARSING_FAIL_COLLATERAL_OUTPUT;
        case TOTAL_COLLATERAL_PARSING_ERROR:    // key 17
            return SWO_TX_PARSING_FAIL_TOTAL_COLLATERAL;
        case REFERENCE_INPUTS_PARSING_ERROR:    // key 18
            return SWO_TX_PARSING_FAIL_REFERENCE_INPUTS;
        case VOTING_PROCEDURES_PARSING_ERROR:   // key 19
            return SWO_TX_PARSING_FAIL_VOTING_PROCEDURES;
        // key 20 is proposal procedures (NOT SUPPORTED - intentionally excluded from Ledger Cardano app)
        case TREASURY_PARSING_ERROR:        // key 21
            return SWO_TX_PARSING_FAIL_TREASURY;
        case DONATION_PARSING_ERROR:        // key 22
            return SWO_TX_PARSING_FAIL_DONATION;
        case CANONICAL_ORDERING_ERROR:
            return SWO_TX_PARSING_FAIL_CANONICAL_ORDER;
        case TX_SIZE_TOO_LARGE_ERROR:
            return SWO_INVALID_TX_LENGTH;
        case TX_BUFFER_NOT_FULLY_CONSUMED_ERROR:
            return SWO_TX_PARSING_FAIL_BUFFER_NOT_FULLY_CONSUMED;
        case OUT_OF_MEMORY_ERROR:
            return SWO_INSUFFICIENT_MEMORY;
    default:
        LEDGER_ASSERT(false, "Unmapped parser error - all cases should be explicit");
        return SWO_TX_PARSING_FAIL;  // fallback if assert is disabled
    }
}

static inline bool canonical_key_ok(bool has_previous,
                                    const uint8_t* previous,
                                    size_t previous_size,
                                    const uint8_t* next,
                                    size_t next_size) {
    if (!has_previous) {
        return true;
    }
    return cbor_mapKeyFulfillsCanonicalOrdering(previous, previous_size, next, next_size);
}

static void free_asset_group_node(output_asset_group_node_t *group_node) {
    if (group_node == NULL) {
        return;
    }
    s_flist_node *token_node = group_node->asset_group.tokens;
    while (token_node != NULL) {
        s_flist_node *token_next = token_node->next;
        app_mem_free(token_node);
        token_node = token_next;
    }
    group_node->asset_group.tokens = NULL;
    app_mem_free(group_node);
}

static void free_asset_groups(s_flist_node *group_node) {
    while (group_node != NULL) {
        s_flist_node *group_next = group_node->next;
        free_asset_group_node((output_asset_group_node_t *) group_node);
        group_node = group_next;
    }
}

static void free_output_item(tx_output_node_t *item) {
    if (item == NULL) {
        return;
    }
    free_asset_groups(item->output_data.assetGroups);
    item->output_data.assetGroups = NULL;
    app_mem_free(item);
}

static void free_mint_item(mint_asset_group_node_t *item) {
    if (item == NULL) {
        return;
    }
    s_flist_node *token_node = item->asset_group.tokens;
    while (token_node != NULL) {
        s_flist_node *token_next = token_node->next;
        app_mem_free(token_node);
        token_node = token_next;
    }
    item->asset_group.tokens = NULL;
    app_mem_free(item);
}

static void free_vote_list(s_flist_node *vote_node) {
    while (vote_node != NULL) {
        s_flist_node *vote_next = vote_node->next;
        app_mem_free(vote_node);
        vote_node = vote_next;
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
        if (!buffer_read_bytes_ptr(buf, &tx->scriptDataHash, SCRIPT_DATA_HASH_LENGTH)) {
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

    // key 19: voting procedures
    if (tx->num_voters > 0) {
        status = parse_tx_voting_procedures(buf, tx);
        if (status != PARSING_OK) {
            return status;
        }
    }

    // key 21: treasury (optional)
    if (tx->includeTreasury) {
        if (!buffer_read_u64(buf, &tx->treasury, BE)) {
            return TREASURY_PARSING_ERROR;
        }
    }

    // key 22: donation (optional)
    if (tx->includeDonation) {
        if (!buffer_read_u64(buf, &tx->donation, BE)) {
            return DONATION_PARSING_ERROR;
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
    uint16_t swo = _map_parser_status_to_swo(status);
    TRACE("tx_handle_parse_error status=%d swo=0x%04x", status, swo);
    return send_swo_and_reset(swo);
}

// Helper function to parse a single input (reused for inputs, collateral inputs, reference inputs)
// error_on_failure: error code to return if parsing fails (e.g., INPUTS_PARSING_ERROR, COLLATERAL_INPUTS_PARSING_ERROR)
static parser_status_e parse_input_item(buffer_t *buf, s_flist_node **list_head, parser_status_e error_on_failure) {
    tx_input_node_t *item = (tx_input_node_t *) app_mem_alloc(sizeof(tx_input_node_t));
    if (item == NULL) {
        return OUT_OF_MEMORY_ERROR;
    }

    // Store pointer to tx hash in raw buffer instead of copying
    if (!buffer_read_bytes_ptr(buf, &item->input.txHash, TX_HASH_LENGTH)) {
        app_mem_free(item);
        return error_on_failure;
    }
    ASSERT(item->input.txHash != NULL);

    if (!buffer_read_u32(buf, &item->input.index, BE)) {
        app_mem_free(item);
        return error_on_failure;
    }

    item->flist_node.next = NULL;
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

        tx_output_node_t *item = (tx_output_node_t *) app_mem_alloc(sizeof(tx_output_node_t));
        if (item == NULL) {
            return OUT_OF_MEMORY_ERROR;
        }
        explicit_bzero(item, sizeof(*item));

        // Parse destination (third-party address or device-owned address params)
        parser_status_e status = parse_output_destination(&output_buf,
                                                          &item->output_data.destination,
                                                          tx->networkId);
        if (status != PARSING_OK) {
            app_mem_free(item);
            return status;
        }
        TRACE("Deserialize: Output %u destination parsed", i);

        if (!buffer_read_u64(&output_buf, &item->output_data.adaAmount, BE)) {
            app_mem_free(item);
            return OUTPUTS_PARSING_ERROR;
        }

        // Parse output format (ARRAY_LEGACY or MAP_BABBAGE)
        status = parse_output_format(&output_buf, &item->output_data.format);
        if (status != PARSING_OK) {
            app_mem_free(item);
            return status;
        }
        TRACE("Deserialize: Output %u format parsed", i);

        if (!buffer_read_u16(&output_buf, &item->output_data.numAssetGroups, BE)) {
            app_mem_free(item);
            return OUTPUTS_PARSING_ERROR;
        }
        TRACE("Deserialize: Output %u: %u asset groups", i, item->output_data.numAssetGroups);

        item->output_data.assetGroups = NULL;
        if (item->output_data.numAssetGroups > 0) {
            const uint8_t* previous_policy_id = NULL;
            bool has_previous_policy = false;

            for (uint16_t ag = 0; ag < item->output_data.numAssetGroups; ag++) {
                output_asset_group_node_t *group_node =
                    (output_asset_group_node_t *) app_mem_alloc(sizeof(output_asset_group_node_t));
                if (group_node == NULL) {
                    free_output_item(item);
                    return OUT_OF_MEMORY_ERROR;
                }
                explicit_bzero(group_node, sizeof(*group_node));

                output_asset_group_t *group = &group_node->asset_group;

                // Store pointer to policy ID in raw buffer instead of copying
                if (!buffer_read_bytes_ptr(&output_buf, &group->policyId, MINTING_POLICY_ID_LENGTH)) {
                    free_asset_group_node(group_node);
                    free_output_item(item);
                    return OUTPUTS_PARSING_ERROR;
                }
                ASSERT(group->policyId != NULL);

                if (!canonical_key_ok(has_previous_policy,
                                      previous_policy_id,
                                      MINTING_POLICY_ID_LENGTH,
                                      group->policyId,
                                      MINTING_POLICY_ID_LENGTH)) {
                    TRACE("Output %u asset groups not canonical", i);
                    free_asset_group_node(group_node);
                    free_output_item(item);
                    return CANONICAL_ORDERING_ERROR;
                }
                previous_policy_id = group->policyId;
                has_previous_policy = true;

                TRACE("Deserialize: Asset group %u: policy ID read", ag);

                if (!buffer_read_u16(&output_buf, &group->numTokens, BE)) {
                    free_asset_group_node(group_node);
                    free_output_item(item);
                    return OUTPUTS_PARSING_ERROR;
                }
                TRACE("Deserialize: Asset group %u: %u tokens", ag, group->numTokens);

                // Initialize tokens linked list
                group->tokens = NULL;

                const uint8_t* previous_token_name = NULL;
                size_t previous_token_len = 0;
                bool has_previous_token = false;

                for (uint16_t tk = 0; tk < group->numTokens; tk++) {
                    // Allocate list node for this token
                    output_token_node_t *token_item =
                        (output_token_node_t *) app_mem_alloc(sizeof(output_token_node_t));
                    if (token_item == NULL) {
                        free_asset_group_node(group_node);
                        free_output_item(item);
                        return OUT_OF_MEMORY_ERROR;
                    }

                    output_token_t *token = &token_item->token_data;
                    if (!buffer_read_u8(&output_buf, &token->assetNameLen)) {
                        app_mem_free(token_item);
                        free_asset_group_node(group_node);
                        free_output_item(item);
                        return OUTPUTS_PARSING_ERROR;
                    }
                    if (token->assetNameLen > MAX_ASSET_NAME_LENGTH) {
                        app_mem_free(token_item);
                        free_asset_group_node(group_node);
                        free_output_item(item);
                        return OUTPUTS_PARSING_ERROR;
                    }

                    // Store pointer to asset name in raw buffer instead of copying
                    // Note: assetNameLen can be 0 for empty asset names, which is valid
                    if (!buffer_read_bytes_ptr(&output_buf, &token->assetName, token->assetNameLen)) {
                        app_mem_free(token_item);
                        free_asset_group_node(group_node);
                        free_output_item(item);
                        return OUTPUTS_PARSING_ERROR;
                    }
                    ASSERT(token->assetName != NULL);

                    if (!canonical_key_ok(has_previous_token,
                                          previous_token_name,
                                          previous_token_len,
                                          token->assetName,
                                          token->assetNameLen)) {
                        TRACE("Output %u asset group %u tokens not canonical", i, ag);
                        app_mem_free(token_item);
                        free_asset_group_node(group_node);
                        free_output_item(item);
                        return CANONICAL_ORDERING_ERROR;
                    }
                    previous_token_name = token->assetName;
                    previous_token_len = token->assetNameLen;
                    has_previous_token = true;

                    if (!buffer_read_u64(&output_buf, &token->amount, BE)) {
                        app_mem_free(token_item);
                        free_asset_group_node(group_node);
                        free_output_item(item);
                        return OUTPUTS_PARSING_ERROR;
                    }

                    TRACE("Deserialize: Token %u: name_len=%u, amount=",
                          tk, token->assetNameLen);
                    TRACE_UINT64(token->amount);

                    // Add token to asset group's token list
                    token_item->flist_node.next = NULL;
                    flist_push_back(&group->tokens, (s_flist_node *) token_item);
                }

                group_node->flist_node.next = NULL;
                flist_push_back(&item->output_data.assetGroups, (s_flist_node *) group_node);
            }
        }

        // Parse datum (hash, inline, or none)
        status = parse_output_datum(&output_buf, &item->output_data.datum);
        if (status != PARSING_OK) {
            free_output_item(item);
            return status;
        }
        TRACE("Deserialize: Output %u datum parsed", i);

        // Parse reference script (if present)
        status = parse_output_ref_script(&output_buf,
                                        &item->output_data.refScript);
        if (status != PARSING_OK) {
            free_output_item(item);
            return status;
        }
        TRACE("Deserialize: Output %u reference script parsed", i);

        // Verify output_buf was fully consumed (nothing more, nothing less)
        if (buffer_can_read(&output_buf, 1)) {
            TRACE("Deserialize: Output %u buffer not fully consumed: offset=%u, size=%u",
                  i, output_buf.offset, output_buf.size);
            free_output_item(item);
            return OUTPUTS_PARSING_ERROR;
        }
        TRACE("Deserialize: Output %u fully consumed, advancing main buffer by %u bytes",
              i, output_len);

        // Advance main buffer past this output
        if (!buffer_seek_cur(buf, output_len)) {
            free_output_item(item);
            return OUTPUTS_PARSING_ERROR;
        }
        TRACE("Deserialize: Output %u complete, buffer offset now=%u", i, buf->offset);

        item->flist_node.next = NULL;
        flist_push_back(&tx->outputs, (s_flist_node *) item);
    }
    return PARSING_OK;
}

static parser_status_e parse_tx_mint_groups(buffer_t *buf, transaction_t *tx) {
    const uint8_t* previous_policy_id = NULL;
    bool has_previous_policy = false;

    for (uint16_t ag = 0; ag < tx->num_mint_asset_groups; ag++) {
        mint_asset_group_node_t *item = (mint_asset_group_node_t *) app_mem_alloc(sizeof(mint_asset_group_node_t));
        if (item == NULL) {
            return OUT_OF_MEMORY_ERROR;
        }
        explicit_bzero(item, sizeof(*item));

        // Store pointer to policy ID in raw buffer instead of copying
        if (!buffer_read_bytes_ptr(buf, &item->asset_group.policyId, MINTING_POLICY_ID_LENGTH)) {
            free_mint_item(item);
            return MINT_PARSING_ERROR;
        }
        ASSERT(item->asset_group.policyId != NULL);
        if (!canonical_key_ok(has_previous_policy,
                              previous_policy_id,
                              MINTING_POLICY_ID_LENGTH,
                              item->asset_group.policyId,
                              MINTING_POLICY_ID_LENGTH)) {
            TRACE("Mint asset groups not canonical");
            free_mint_item(item);
            return CANONICAL_ORDERING_ERROR;
        }
        previous_policy_id = item->asset_group.policyId;
        has_previous_policy = true;
        TRACE("Deserialize: Mint asset group %u: policy ID read", ag);

        if (!buffer_read_u16(buf, &item->asset_group.numTokens, BE)) {
            free_mint_item(item);
            return MINT_PARSING_ERROR;
        }
        TRACE("Deserialize: Mint asset group %u: %u tokens", ag, item->asset_group.numTokens);

        // Initialize tokens linked list
        item->asset_group.tokens = NULL;

        const uint8_t* previous_token_name = NULL;
        size_t previous_token_len = 0;
        bool has_previous_token = false;

        for (uint16_t tk = 0; tk < item->asset_group.numTokens; tk++) {
            // Allocate list node for this token
            mint_token_node_t *token_item = (mint_token_node_t *) app_mem_alloc(sizeof(mint_token_node_t));
            if (token_item == NULL) {
                free_mint_item(item);
                return OUT_OF_MEMORY_ERROR;
            }

            mint_token_t *token = &token_item->token;
            if (!buffer_read_u8(buf, &token->assetNameLen)) {
                app_mem_free(token_item);
                free_mint_item(item);
                return MINT_PARSING_ERROR;
            }
            if (token->assetNameLen > MAX_MINT_ASSET_NAME_LENGTH) {
                app_mem_free(token_item);
                free_mint_item(item);
                return MINT_PARSING_ERROR;
            }

            // Store pointer to asset name in raw buffer instead of copying
            // Note: assetNameLen can be 0 for empty asset names, which is valid
            if (!buffer_read_bytes_ptr(buf, &token->assetName, token->assetNameLen)) {
                app_mem_free(token_item);
                free_mint_item(item);
                return MINT_PARSING_ERROR;
            }
            ASSERT(token->assetName != NULL);

            if (!canonical_key_ok(has_previous_token,
                                  previous_token_name,
                                  previous_token_len,
                                  token->assetName,
                                  token->assetNameLen)) {
                TRACE("Mint asset group %u tokens not canonical", ag);
                app_mem_free(token_item);
                free_mint_item(item);
                return CANONICAL_ORDERING_ERROR;
            }
            previous_token_name = token->assetName;
            previous_token_len = token->assetNameLen;
            has_previous_token = true;

            if (!buffer_read_int64(buf, &token->amount, BE)) {
                app_mem_free(token_item);
                free_mint_item(item);
                return MINT_PARSING_ERROR;
            }

            TRACE("Deserialize: Mint token %u: name_len=%u, amount=", tk, token->assetNameLen);
            TRACE_INT64(token->amount);

            // Add token to asset group's token list
            token_item->flist_node.next = NULL;
            flist_push_back(&item->asset_group.tokens, (s_flist_node *) token_item);
        }

        item->flist_node.next = NULL;
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
        tx_certificate_node_t *item = (tx_certificate_node_t *) app_mem_alloc(sizeof(tx_certificate_node_t));
        if (item == NULL) {
            TRACE(">>>>> OUT OF MEMORY");
            return OUT_OF_MEMORY_ERROR;
        }

        // Read certificate type
        uint8_t cert_type_wire;
        TRACE(">>>>> About to read certificate type byte at offset %u", buf->offset);
        if (!buffer_read_u8(buf, &cert_type_wire)) {
            TRACE(">>>>> FAILED TO READ CERTIFICATE TYPE BYTE");
            app_mem_free(item);
            return CERTIFICATES_PARSING_ERROR;
        }
        certificate_type_t cert_type = (certificate_type_t) cert_type_wire;
        TRACE("Deserialize: Certificate %u type=%u", i, cert_type_wire);

        // Parse certificate data based on type
        parser_status_e status = PARSING_OK;
        switch (cert_type) {
            case CERTIFICATE_STAKE_REGISTRATION:
            case CERTIFICATE_STAKE_DEREGISTRATION:
                status = parse_certificate_stake_registration_deregistration(buf, cert_type, &item->certificate);
                break;

            case CERTIFICATE_STAKE_DELEGATION:
                status = parse_certificate_stake_delegation(buf, &item->certificate);
                break;

            case CERTIFICATE_STAKE_REGISTRATION_CONWAY:
            case CERTIFICATE_STAKE_DEREGISTRATION_CONWAY:
                status = parse_certificate_stake_registration_deregistration_conway(buf, cert_type, &item->certificate);
                break;

            case CERTIFICATE_STAKE_POOL_RETIREMENT:
                status = parse_certificate_stake_pool_retirement(buf, &item->certificate);
                break;

            case CERTIFICATE_VOTE_DELEGATION:
                status = parse_certificate_vote_delegation(buf, &item->certificate);
                break;

            case CERTIFICATE_AUTHORIZE_COMMITTEE_HOT:
                status = parse_certificate_authorize_committee_hot(buf, &item->certificate);
                break;

            case CERTIFICATE_RESIGN_COMMITTEE_COLD:
                status = parse_certificate_resign_committee_cold(buf, &item->certificate);
                break;

            case CERTIFICATE_DREP_REGISTRATION:
                status = parse_certificate_drep_registration(buf, &item->certificate);
                break;

            case CERTIFICATE_DREP_DEREGISTRATION:
                status = parse_certificate_drep_deregistration(buf, &item->certificate);
                break;

            case CERTIFICATE_DREP_UPDATE:
                status = parse_certificate_drep_update(buf, &item->certificate);
                break;

            case CERTIFICATE_STAKE_POOL_REGISTRATION:
                status = parse_certificate_stake_pool_registration(buf, &item->certificate);
                break;

        default:
            // Unknown certificate type
            status = CERTIFICATES_PARSING_ERROR;
            break;
        }

        if (status != PARSING_OK) {
            TRACE("Certificate parse failure: type=%u status=%d", cert_type_wire, status);
            app_mem_free(item);
            return status;
        }

        item->flist_node.next = NULL;
        flist_push_back(&tx->certificates, (s_flist_node *) item);
    }
    return PARSING_OK;
}

static parser_status_e parse_tx_withdrawals(buffer_t *buf, transaction_t *tx) {
    // Withdrawals are serialized as a canonical CBOR map keyed by reward accounts.
    // Building those addresses here would require deriving them before the security
    // policies run, so the canonical-order enforcement for withdrawals is postponed
    // to the later planning stage where the derived reward addresses are already
    // exposed to policy checks.
    for (uint16_t i = 0; i < tx->num_withdrawals; i++) {
        tx_withdrawal_node_t *item = (tx_withdrawal_node_t *) app_mem_alloc(sizeof(tx_withdrawal_node_t));
        if (item == NULL) {
            return OUT_OF_MEMORY_ERROR;
        }

        if (!buffer_read_u64(buf, &item->withdrawal.amount, BE)) {
            app_mem_free(item);
            return WITHDRAWALS_PARSING_ERROR;
        }

        parser_status_e status = parse_stake_credential(buf, &item->withdrawal.stakeCredential);
        if (status != PARSING_OK) {
            TRACE("Withdrawal %u credential parsing failed: status=%d", i, status);
            app_mem_free(item);
            return status;
        }

        TRACE("Deserialize: Withdrawal %u, type=%u", i, item->withdrawal.stakeCredential.type);

        item->flist_node.next = NULL;
        flist_push_back(&tx->withdrawals, (s_flist_node *) item);
    }
    return PARSING_OK;
}

/// Clean up dynamically allocated memory in transaction outputs (including list items)
void transaction_free_outputs(transaction_t *tx) {
    LEDGER_ASSERT(tx != NULL, "NULL tx");

    s_flist_node *output_node = tx->outputs;
    while (output_node != NULL) {
        tx_output_node_t *item = (tx_output_node_t *) output_node;
        s_flist_node *next = output_node->next;

        // Free asset groups and their tokens
        free_asset_groups(item->output_data.assetGroups);

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
        mint_asset_group_node_t *item = (mint_asset_group_node_t *) mint_node;
        s_flist_node *next = mint_node->next;

        // Free all token nodes in the linked list
        s_flist_node *token_node = item->asset_group.tokens;
        while (token_node != NULL) {
            s_flist_node *token_next = token_node->next;
            app_mem_free(token_node);
            token_node = token_next;
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

void transaction_free_voting_procedures(transaction_t *tx) {
    LEDGER_ASSERT(tx != NULL, "NULL tx");

    s_flist_node *voter_node = tx->voting_procedures;
    while (voter_node != NULL) {
        voter_votes_node_t *voter_item = (voter_votes_node_t *) voter_node;
        s_flist_node *next = voter_node->next;
        free_vote_list(voter_item->voter_votes_data.votes);
        app_mem_free(voter_node);
        voter_node = next;
    }
    tx->voting_procedures = NULL;
}

void transaction_free_collateral_output(transaction_t *tx) {
    LEDGER_ASSERT(tx != NULL, "NULL tx");

    free_asset_groups(tx->collateral_output.assetGroups);
    tx->collateral_output.assetGroups = NULL;
    tx->collateral_output.numAssetGroups = 0;
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

    // key 19: voting procedures
    transaction_free_voting_procedures(tx);

    // key 16: collateral output
    transaction_free_collateral_output(tx);

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
        tx_required_signer_node_t *item =
            (tx_required_signer_node_t *) app_mem_alloc(sizeof(tx_required_signer_node_t));
        if (item == NULL) {
            return OUT_OF_MEMORY_ERROR;
        }

        // Read signer type (1 byte)
        uint8_t type;
        if (!buffer_read_u8(buf, &type)) {
            app_mem_free(item);
            return REQUIRED_SIGNERS_PARSING_ERROR;
        }
        item->required_signer.type = (required_signer_type_t) type;

        // Read path or hash based on type
        switch (item->required_signer.type) {
            case REQUIRED_SIGNER_WITH_PATH:
                // Parse BIP44 path
                if (!buffer_read_bip44_path(buf, &item->required_signer.keyPath)) {
                    app_mem_free(item);
                    return REQUIRED_SIGNERS_PARSING_ERROR;
                }
                break;
            case REQUIRED_SIGNER_WITH_HASH:
                // Read 28-byte key hash
                if (!buffer_read_bytes_ptr(buf, &item->required_signer.keyHash, ADDRESS_KEY_HASH_LENGTH)) {
                    app_mem_free(item);
                    return REQUIRED_SIGNERS_PARSING_ERROR;
                }
                ASSERT(item->required_signer.keyHash != NULL);
                break;
            default:
                app_mem_free(item);
                return REQUIRED_SIGNERS_PARSING_ERROR;
        }

        item->flist_node.next = NULL;
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
                                             s_flist_node **assetGroups,
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

    *assetGroups = NULL;
    if (*numAssetGroups > 0) {
        const uint8_t* previous_policy_id = NULL;
        bool has_previous_policy = false;

        for (uint16_t ag = 0; ag < *numAssetGroups; ag++) {
            output_asset_group_node_t *group_node =
                (output_asset_group_node_t *) app_mem_alloc(sizeof(output_asset_group_node_t));
            if (group_node == NULL) {
                free_asset_groups(*assetGroups);
                return OUT_OF_MEMORY_ERROR;
            }
            explicit_bzero(group_node, sizeof(*group_node));

            output_asset_group_t *group = &group_node->asset_group;

            if (!buffer_read_bytes_ptr(output_buf, &group->policyId, MINTING_POLICY_ID_LENGTH)) {
                free_asset_group_node(group_node);
                free_asset_groups(*assetGroups);
                return OUTPUTS_PARSING_ERROR;
            }
            ASSERT(group->policyId != NULL);

            if (!canonical_key_ok(has_previous_policy,
                                  previous_policy_id,
                                  MINTING_POLICY_ID_LENGTH,
                                  group->policyId,
                                  MINTING_POLICY_ID_LENGTH)) {
                TRACE("Collateral asset groups not canonical");
                free_asset_group_node(group_node);
                free_asset_groups(*assetGroups);
                return CANONICAL_ORDERING_ERROR;
            }
            previous_policy_id = group->policyId;
            has_previous_policy = true;

            if (!buffer_read_u16(output_buf, &group->numTokens, BE)) {
                free_asset_group_node(group_node);
                free_asset_groups(*assetGroups);
                return OUTPUTS_PARSING_ERROR;
            }

            // Initialize tokens linked list
            group->tokens = NULL;

            const uint8_t* previous_token_name = NULL;
            size_t previous_token_len = 0;
            bool has_previous_token = false;

            for (uint16_t tk = 0; tk < group->numTokens; tk++) {
                // Allocate list node for this token
                output_token_node_t *token_item =
                    (output_token_node_t *) app_mem_alloc(sizeof(output_token_node_t));
                if (token_item == NULL) {
                    free_asset_group_node(group_node);
                    free_asset_groups(*assetGroups);
                    return OUT_OF_MEMORY_ERROR;
                }

                output_token_t *token = &token_item->token_data;
                if (!buffer_read_u8(output_buf, &token->assetNameLen)) {
                    app_mem_free(token_item);
                    free_asset_group_node(group_node);
                    free_asset_groups(*assetGroups);
                    return OUTPUTS_PARSING_ERROR;
                }
                if (token->assetNameLen > MAX_ASSET_NAME_LENGTH) {
                    app_mem_free(token_item);
                    free_asset_group_node(group_node);
                    free_asset_groups(*assetGroups);
                    return OUTPUTS_PARSING_ERROR;
                }

                if (!buffer_read_bytes_ptr(output_buf, &token->assetName, token->assetNameLen)) {
                    app_mem_free(token_item);
                    free_asset_group_node(group_node);
                    free_asset_groups(*assetGroups);
                    return OUTPUTS_PARSING_ERROR;
                }
                ASSERT(token->assetName != NULL);

                if (!canonical_key_ok(has_previous_token,
                                      previous_token_name,
                                      previous_token_len,
                                      token->assetName,
                                      token->assetNameLen)) {
                    TRACE("Collateral asset group %u tokens not canonical", ag);
                    app_mem_free(token_item);
                    free_asset_group_node(group_node);
                    free_asset_groups(*assetGroups);
                    return CANONICAL_ORDERING_ERROR;
                }
                previous_token_name = token->assetName;
                previous_token_len = token->assetNameLen;
                has_previous_token = true;

                if (!buffer_read_u64(output_buf, &token->amount, BE)) {
                    app_mem_free(token_item);
                    free_asset_group_node(group_node);
                    free_asset_groups(*assetGroups);
                    return OUTPUTS_PARSING_ERROR;
                }

                // Add token to asset group's token list
                token_item->flist_node.next = NULL;
                flist_push_back(&group->tokens, (s_flist_node *) token_item);
            }

            group_node->flist_node.next = NULL;
            flist_push_back(assetGroups, (s_flist_node *) group_node);
        }
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

    status = parse_output_datum(&output_buf, &tx->collateral_output.datum);
    if (status != PARSING_OK) {
        return status;
    }

    status = parse_output_ref_script(&output_buf,
                                       &tx->collateral_output.refScript);
    if (status != PARSING_OK) {
        return status;
    }

    buf->offset += output_len;
    return PARSING_OK;
}

static parser_status_e parse_tx_voting_procedures(buffer_t *buf, transaction_t *tx) {
    // The voter list is defined as a canonical CBOR map keyed by voters. The
    // canonical ordering cannot be enforced here because the voter key encoding
    // depends on the credential type (paths would need to be hashed/derived),
    // and those derived bytes are not exposed before security policies execute.
    // Validating the canonical order therefore happens later (see
    // tx_validate_and_compute_hash) after policy checks have already derived
    // the voter keys.
    // For each voter in the outer map
    for (uint16_t voter_idx = 0; voter_idx < tx->num_voters; voter_idx++) {
        // Allocate list node for this voter
        voter_votes_node_t *voter_item =
            (voter_votes_node_t *) app_mem_alloc(sizeof(voter_votes_node_t));
        if (voter_item == NULL) {
            return OUT_OF_MEMORY_ERROR;
        }
        explicit_bzero(voter_item, sizeof(*voter_item));

        // Initialize votes list
        voter_item->voter_votes_data.votes = NULL;

        // Parse voter (ext_voter_t)
        uint8_t voter_type_byte;
        if (!buffer_read_u8(buf, &voter_type_byte)) {
            return VOTING_PROCEDURES_PARSING_ERROR;
        }
        voter_item->voter_votes_data.voter.type = (ext_voter_type_t) voter_type_byte;

        // Parse voter key/hash based on type
        switch (voter_item->voter_votes_data.voter.type) {
            case EXT_VOTER_COMMITTEE_HOT_KEY_PATH:
            case EXT_VOTER_DREP_KEY_PATH:
            case EXT_VOTER_STAKE_POOL_KEY_PATH:
                if (!buffer_read_bip44_path(buf, &voter_item->voter_votes_data.voter.keyPath)) {
                    return VOTING_PROCEDURES_PARSING_ERROR;
                }
                break;

            case EXT_VOTER_COMMITTEE_HOT_KEY_HASH:
            case EXT_VOTER_DREP_KEY_HASH:
            case EXT_VOTER_STAKE_POOL_KEY_HASH:
                if (!buffer_read_bytes_ptr(buf, &voter_item->voter_votes_data.voter.keyHash,
                                           ADDRESS_KEY_HASH_LENGTH)) {
                    return VOTING_PROCEDURES_PARSING_ERROR;
                }
                break;

            case EXT_VOTER_COMMITTEE_HOT_SCRIPT_HASH:
            case EXT_VOTER_DREP_SCRIPT_HASH:
                if (!buffer_read_bytes_ptr(buf, &voter_item->voter_votes_data.voter.scriptHash,
                                           SCRIPT_HASH_LENGTH)) {
                    return VOTING_PROCEDURES_PARSING_ERROR;
                }
                break;

            default:
                return VOTING_PROCEDURES_PARSING_ERROR;
        }

        if (!buffer_read_u16(buf, &voter_item->voter_votes_data.numVotes, BE)) {
            return VOTING_PROCEDURES_PARSING_ERROR;
        }

        // Parse each vote for this voter
        for (uint16_t vote_idx = 0; vote_idx < voter_item->voter_votes_data.numVotes; vote_idx++) {
            // Allocate list node for this vote
            vote_node_t *vote_item =
                (vote_node_t *) app_mem_alloc(sizeof(vote_node_t));
            if (vote_item == NULL) {
                return OUT_OF_MEMORY_ERROR;
            }
            explicit_bzero(vote_item, sizeof(*vote_item));

            // Parse gov_action_id (tx_hash + index)
            if (!buffer_read_bytes_ptr(buf, &vote_item->vote_data.govActionId.txHash, TX_HASH_LENGTH)) {
                return VOTING_PROCEDURES_PARSING_ERROR;
            }
            ASSERT(vote_item->vote_data.govActionId.txHash != NULL);

            if (!buffer_read_u32(buf, &vote_item->vote_data.govActionId.govActionIndex, BE)) {
                return VOTING_PROCEDURES_PARSING_ERROR;
            }
            // Parse voting_procedure (vote + optional anchor)
            uint8_t vote_byte;
            if (!buffer_read_u8(buf, &vote_byte)) {
                return VOTING_PROCEDURES_PARSING_ERROR;
            }
            vote_item->vote_data.voteOption = (vote_t) vote_byte;

            // Parse anchor inclusion flag using parseIncluded pattern
            uint8_t anchor_included_byte;
            if (!buffer_read_u8(buf, &anchor_included_byte)) {
                return VOTING_PROCEDURES_PARSING_ERROR;
            }
            if (!parseIncluded(anchor_included_byte, &vote_item->vote_data.anchor.isIncluded)) {
                return VOTING_PROCEDURES_PARSING_ERROR;
            }

            if (vote_item->vote_data.anchor.isIncluded) {
                // Parse URL length and pointer
                uint16_t url_len;
                if (!buffer_read_u16(buf, &url_len, BE)) {
                    return VOTING_PROCEDURES_PARSING_ERROR;
                }
                vote_item->vote_data.anchor.urlLength = url_len;

                if (!buffer_read_bytes_ptr(buf, &vote_item->vote_data.anchor.url, url_len)) {
                    return VOTING_PROCEDURES_PARSING_ERROR;
                }
                ASSERT(vote_item->vote_data.anchor.url != NULL);

                // Parse hash (32 bytes)
                if (!buffer_read_bytes_ptr(buf, &vote_item->vote_data.anchor.hash, ANCHOR_HASH_LENGTH)) {
                    return VOTING_PROCEDURES_PARSING_ERROR;
                }
                ASSERT(vote_item->vote_data.anchor.hash != NULL);
            }

            // Add vote to voter's vote list
            vote_item->flist_node.next = NULL;
            flist_push_back(&voter_item->voter_votes_data.votes, (s_flist_node *) vote_item);
        }

        // Add voter to transaction's voter list
        voter_item->flist_node.next = NULL;
        flist_push_back(&tx->voting_procedures, (s_flist_node *) voter_item);
    }

    return PARSING_OK;
}

// Reference inputs parsing is inline in parse_tx() since they use the same format as regular inputs
