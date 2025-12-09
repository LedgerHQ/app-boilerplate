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

#include "tx_parse.h"
#include "utils.h"
#include "utils/assert.h"
#include "utils/textUtils.h"
#include "utils/buffer_utils.h"
#include "types.h"
#include "tx_output_types.h"
#include "addressUtils/addressUtilsShelley.h"
#include "globals.h"

parser_status_e parse_tx(buffer_t *buf, transaction_t *tx) {
    LEDGER_ASSERT(buf != NULL, "NULL buf");
    LEDGER_ASSERT(tx != NULL, "NULL tx");

    if (buf->size > MAX_TX_LEN) {
        return WRONG_LENGTH_ERROR;
    }

    // Initialize inputs list
    tx->inputs = NULL;

    // Initialize withdrawals list
    tx->withdrawals = NULL;

    // Initialize mint list
    tx->mint_asset_groups = NULL;

    // Note: num_inputs, num_outputs, num_withdrawals, and num_mint_asset_groups are already set from INIT APDU, not read from buffer

    // Parse each input and add to linked list
    for (uint16_t i = 0; i < tx->num_inputs; i++) {
        // Allocate memory for the input list item
        tx_input_list_item_t *item = (tx_input_list_item_t *) app_mem_alloc(sizeof(tx_input_list_item_t));
        if (item == NULL) {
            return INPUTS_PARSING_ERROR;
        }

        // Get pointer to transaction hash (32 bytes) and copy it
        uint8_t *txHash = (uint8_t *) (buf->ptr + buf->offset);
        if (!buffer_seek_cur(buf, TX_HASH_LENGTH)) {
            return INPUTS_PARSING_ERROR;
        }
        memmove(item->input_data.txHashBuffer, txHash, TX_HASH_LENGTH);

        // Read output index (uint32, big-endian)
        if (!buffer_read_u32(buf, &item->input_data.index, BE)) {
            return INPUTS_PARSING_ERROR;
        }

        // Add to linked list
        item->node.next = NULL;
        flist_push_back(&tx->inputs, (s_flist_node *) item);
    }

    // Initialize outputs list
    tx->outputs = NULL;

    // Parse each output and add to linked list
    for (uint16_t i = 0; i < tx->num_outputs; i++) {
        // Read output length (uint16, big-endian)
        uint16_t output_len;
        if (!buffer_read_u16(buf, &output_len, BE)) {
            return OUTPUTS_PARSING_ERROR;
        }

        // Track starting position for this output
        size_t output_start = buf->offset;

        // Allocate memory for the output list item
        tx_output_list_item_t *item = (tx_output_list_item_t *) app_mem_alloc(sizeof(tx_output_list_item_t));
        if (item == NULL) {
            return OUTPUTS_PARSING_ERROR;
        }

        // Read destination type (uint8)
        uint8_t dest_type;
        if (!buffer_read_u8(buf, &dest_type)) {
            return OUTPUTS_PARSING_ERROR;
        }
        TRACE("Deserialize: Output %u destination type=0x%02x (1=THIRD_PARTY, 2=DEVICE_OWNED)", i, dest_type);
        item->output_data.destination.type = (tx_output_destination_type_t) dest_type;

        if (dest_type == DESTINATION_THIRD_PARTY) {
            // Read address size (uint16, big-endian)
            uint16_t addr_size;
            if (!buffer_read_u16(buf, &addr_size, BE)) {
                return OUTPUTS_PARSING_ERROR;
            }

            // Validate address size
            if (addr_size == 0 || addr_size > MAX_ADDRESS_SIZE) {
                return OUTPUT_ADDRESS_SIZE_ERROR;
            }
            item->output_data.destination.address.size = addr_size;

            // Read address bytes
            uint8_t *addr = (uint8_t *) (buf->ptr + buf->offset);
            if (!buffer_seek_cur(buf, addr_size)) {
                return OUTPUTS_PARSING_ERROR;
            }
            memmove(item->output_data.destination.address.buffer, addr, addr_size);

        } else if (dest_type == DESTINATION_DEVICE_OWNED) {
            // Read address type (uint8)
            uint8_t addr_type;
            if (!buffer_read_u8(buf, &addr_type)) {
                return OUTPUTS_PARSING_ERROR;
            }
            item->output_data.destination.params.type = (address_type_t) addr_type;

            // For Byron addresses, read the protocol magic from the output
            // For Shelley addresses, the network ID comes only from the transaction init (tx->networkId)
            // This ensures all outputs use consistent network parameters
            if (addr_type == BYRON) {
                uint32_t protocol_magic;
                if (!buffer_read_u32(buf, &protocol_magic, BE)) {
                    return OUTPUTS_PARSING_ERROR;
                }
                item->output_data.destination.params.protocolMagic = protocol_magic;
            } else {
                // Shelley addresses: network parameters come from transaction, not from output data
                item->output_data.destination.params.networkId = tx->networkId;
            }

            // Read payment credential (path or script hash) based on address type
            // Address type encodes whether payment is key path or script hash
            payment_choice_t payment_choice = determinePaymentChoice(item->output_data.destination.params.type);

            switch (payment_choice) {
                case PAYMENT_PATH: {
                    // Read payment key path using bip44 wire format
                    if (!buffer_read_bip44_path(buf, &item->output_data.destination.params.paymentKeyPath)) {
                        return OUTPUTS_PARSING_ERROR;
                    }
                    TRACE("Deserialize: Payment path, length=%u", item->output_data.destination.params.paymentKeyPath.length);
                    break;
                }
                case PAYMENT_SCRIPT_HASH: {
                    // Read payment script hash (fixed 28 bytes, no length prefix as it's a constant)
                    uint8_t *payment_hash = (uint8_t *) (buf->ptr + buf->offset);
                    if (!buffer_seek_cur(buf, SCRIPT_HASH_LENGTH)) {
                        return OUTPUTS_PARSING_ERROR;
                    }
                    memmove(item->output_data.destination.params.paymentScriptHash, payment_hash, SCRIPT_HASH_LENGTH);
                    TRACE("Deserialize: Payment script hash");
                    break;
                }
                case PAYMENT_NONE:
                    // Reward addresses should not reach here (checked earlier)
                    return OUTPUTS_PARSING_ERROR;
            }

            // Read staking choice (1 byte)
            uint8_t staking_choice;
            if (!buffer_read_u8(buf, &staking_choice)) {
                return OUTPUTS_PARSING_ERROR;
            }
            item->output_data.destination.params.stakingDataSource = (staking_data_source_t) staking_choice;

            // Read staking credential based on staking choice
            switch (staking_choice) {
                case STAKING_KEY_PATH: {
                    if (!buffer_read_bip44_path(buf, &item->output_data.destination.params.stakingKeyPath)) {
                        return OUTPUTS_PARSING_ERROR;
                    }
                    break;
                }
                case STAKING_KEY_HASH: {
                    uint8_t *hash_ptr = (uint8_t *) (buf->ptr + buf->offset);
                    if (!buffer_seek_cur(buf, ADDRESS_KEY_HASH_LENGTH)) {
                        return OUTPUTS_PARSING_ERROR;
                    }
                    memmove(item->output_data.destination.params.stakingKeyHash, hash_ptr, ADDRESS_KEY_HASH_LENGTH);
                    break;
                }
                case STAKING_SCRIPT_HASH: {
                    uint8_t *hash_ptr = (uint8_t *) (buf->ptr + buf->offset);
                    if (!buffer_seek_cur(buf, SCRIPT_HASH_LENGTH)) {
                        return OUTPUTS_PARSING_ERROR;
                    }
                    memmove(item->output_data.destination.params.stakingScriptHash, hash_ptr, SCRIPT_HASH_LENGTH);
                    break;
                }
                case BLOCKCHAIN_POINTER: {
                    uint8_t *ptr_data = (uint8_t *) (buf->ptr + buf->offset);
                    if (!buffer_seek_cur(buf, sizeof(blockchainPointer_t))) {
                        return OUTPUTS_PARSING_ERROR;
                    }
                    memmove(&item->output_data.destination.params.stakingKeyBlockchainPointer,
                           ptr_data,
                           sizeof(blockchainPointer_t));
                    break;
                }
                case NO_STAKING:
                    // No staking credential needed
                    break;
                default:
                    return OUTPUTS_PARSING_ERROR;
            }

        } else {
            return OUTPUT_DESTINATION_TYPE_ERROR;
        }

        // Read ADA amount (uint64, big-endian)
        if (!buffer_read_u64(buf, &item->output_data.adaAmount, BE)) {
            return OUTPUTS_PARSING_ERROR;
        }

        // Read output format (uint8: 0=ARRAY_LEGACY, 1=MAP_BABBAGE)
        uint8_t output_format;
        if (!buffer_read_u8(buf, &output_format)) {
            return OUTPUTS_PARSING_ERROR;
        }
        // Validate format value
        if (output_format != ARRAY_LEGACY && output_format != MAP_BABBAGE) {
            return OUTPUTS_PARSING_ERROR;
        }
        item->output_data.format = (tx_output_serialization_format_t) output_format;
        TRACE("Deserialize: Output %u format=%u", i, output_format);

        // Read number of asset groups (uint16, BE)
        if (!buffer_read_u16(buf, &item->output_data.numAssetGroups, BE)) {
            return OUTPUTS_PARSING_ERROR;
        }
        TRACE("Deserialize: Output %u: %u asset groups", i, item->output_data.numAssetGroups);

        // Validate asset group count
        if (item->output_data.numAssetGroups > MAX_ASSET_GROUPS_PER_OUTPUT) {
            return OUTPUTS_PARSING_ERROR;
        }

        // Allocate asset groups if present
        if (item->output_data.numAssetGroups > 0) {
            item->output_data.assetGroups = (asset_group_t *) app_mem_alloc(
                item->output_data.numAssetGroups * sizeof(asset_group_t));
            if (item->output_data.assetGroups == NULL) {
                return OUTPUTS_PARSING_ERROR;
            }

            // Parse each asset group
            for (uint16_t ag = 0; ag < item->output_data.numAssetGroups; ag++) {
                asset_group_t *group = &item->output_data.assetGroups[ag];

                // Read policy ID (no length prefix)
                uint8_t *policy_id = (uint8_t *) (buf->ptr + buf->offset);
                if (!buffer_seek_cur(buf, MINTING_POLICY_ID_SIZE)) {
                    return OUTPUTS_PARSING_ERROR;
                }
                memmove(group->policyId, policy_id, MINTING_POLICY_ID_SIZE);
                TRACE("Deserialize: Asset group %u: policy ID read", ag);

                // Read number of tokens (uint16, BE)
                if (!buffer_read_u16(buf, &group->numTokens, BE)) {
                    return OUTPUTS_PARSING_ERROR;
                }
                if (group->numTokens > MAX_TOKENS_PER_ASSET_GROUP) {
                    return OUTPUTS_PARSING_ERROR;
                }
                TRACE("Deserialize: Asset group %u: %u tokens", ag, group->numTokens);

                // Allocate tokens array
                group->tokens = (output_token_t *) app_mem_alloc(group->numTokens * sizeof(output_token_t));
                if (group->tokens == NULL) {
                    return OUTPUTS_PARSING_ERROR;
                }

                // Parse each token
                for (uint16_t tk = 0; tk < group->numTokens; tk++) {
                    output_token_t *token = &group->tokens[tk];

                    // Read asset name length (uint8)
                    if (!buffer_read_u8(buf, &token->assetNameLen)) {
                        return OUTPUTS_PARSING_ERROR;
                    }
                    if (token->assetNameLen > 32) {
                        return OUTPUTS_PARSING_ERROR;
                    }

                    // Read asset name (variable length)
                    if (token->assetNameLen > 0) {
                        uint8_t *asset_name = (uint8_t *) (buf->ptr + buf->offset);
                        if (!buffer_seek_cur(buf, token->assetNameLen)) {
                            return OUTPUTS_PARSING_ERROR;
                        }
                        memmove(token->assetName, asset_name, token->assetNameLen);
                    }

                    // Read token amount (int64, BE, signed)
                    if (!buffer_read_u64(buf, (uint64_t*)&token->amount, BE)) {
                        return OUTPUTS_PARSING_ERROR;
                    }

                    TRACE("Deserialize: Token %u: name_len=%u, amount=",
                          tk, token->assetNameLen);
                    TRACE_INT64(token->amount);
                }
            }
        } else {
            item->output_data.assetGroups = NULL;
        }

        // Read datum type (uint8)
        // Wire format: 0=NONE, 1=HASH, 2=INLINE
        // Internal format: 0xFF=NONE, 0=DATUM_HASH, 1=DATUM_INLINE
        uint8_t datum_wire_type;
        if (!buffer_read_u8(buf, &datum_wire_type)) {
            return OUTPUTS_PARSING_ERROR;
        }
        TRACE("Deserialize: Output %u: Datum wire type=%u", i, datum_wire_type);

        // Map wire format to internal format
        if (datum_wire_type == 0) {
            // NONE - set marker value
            item->output_data.datum.type = (datum_type_t) 0xFF;
        } else if (datum_wire_type == 1) {
            // HASH - maps to DATUM_HASH (0)
            item->output_data.datum.type = DATUM_HASH;
        } else if (datum_wire_type == 2) {
            // INLINE - maps to DATUM_INLINE (1)
            item->output_data.datum.type = DATUM_INLINE;
        } else {
            return OUTPUTS_PARSING_ERROR;
        }

        switch (datum_wire_type) {
            case 0:  // NONE
                // No datum
                break;

            case 1:  // HASH
                // Read datum hash
                {
                    uint8_t *hash_ptr = (uint8_t *) (buf->ptr + buf->offset);
                    if (!buffer_seek_cur(buf, OUTPUT_DATUM_HASH_LENGTH)) {
                        return OUTPUTS_PARSING_ERROR;
                    }
                    memmove(item->output_data.datum.hash, hash_ptr, OUTPUT_DATUM_HASH_LENGTH);
                    TRACE("Deserialize: Datum hash read");
                }
                break;

            case 2:  // INLINE
                // Read inline datum size (uint16, BE)
                {
                    uint16_t datum_size;
                    if (!buffer_read_u16(buf, &datum_size, BE)) {
                        return OUTPUTS_PARSING_ERROR;
                    }
                    if (datum_size > MAX_DATUM_INLINE_SIZE) {
                        return OUTPUTS_PARSING_ERROR;
                    }
                    item->output_data.datum.inline_data.size = datum_size;

                    // Allocate and read inline datum data
                    item->output_data.datum.inline_data.data =
                        (uint8_t *) app_mem_alloc(datum_size);
                    if (item->output_data.datum.inline_data.data == NULL) {
                        return OUTPUTS_PARSING_ERROR;
                    }

                    uint8_t *datum_ptr = (uint8_t *) (buf->ptr + buf->offset);
                    if (!buffer_seek_cur(buf, datum_size)) {
                        return OUTPUTS_PARSING_ERROR;
                    }
                    memmove(item->output_data.datum.inline_data.data, datum_ptr, datum_size);
                    TRACE("Deserialize: Inline datum read: %u bytes", datum_size);
                }
                break;

            default:
                return OUTPUTS_PARSING_ERROR;
        }

        // Read hasRefScript flag (uint8)
        uint8_t has_ref_script;
        if (!buffer_read_u8(buf, &has_ref_script)) {
            return OUTPUTS_PARSING_ERROR;
        }
        item->output_data.hasRefScript = (has_ref_script != 0);
        TRACE("Deserialize: Output %u: Has reference script=%u", i, has_ref_script);

        if (item->output_data.hasRefScript) {
            // Read reference script size (uint16, BE)
            uint16_t script_size;
            if (!buffer_read_u16(buf, &script_size, BE)) {
                return OUTPUTS_PARSING_ERROR;
            }
            if (script_size > MAX_REF_SCRIPT_SIZE) {
                return OUTPUTS_PARSING_ERROR;
            }
            item->output_data.refScript.size = script_size;

            // Allocate and read reference script data
            item->output_data.refScript.data = (uint8_t *) app_mem_alloc(script_size);
            if (item->output_data.refScript.data == NULL) {
                return OUTPUTS_PARSING_ERROR;
            }

            uint8_t *script_ptr = (uint8_t *) (buf->ptr + buf->offset);
            if (!buffer_seek_cur(buf, script_size)) {
                return OUTPUTS_PARSING_ERROR;
            }
            memmove(item->output_data.refScript.data, script_ptr, script_size);
            TRACE("Deserialize: Reference script read: %u bytes", script_size);
        }

        // Verify we consumed exactly output_len bytes
        if (buf->offset - output_start != output_len) {
            TRACE("Deserialize: Output length mismatch: expected=%u, consumed=%u",
                  output_len, (uint32_t)(buf->offset - output_start));
            return OUTPUTS_PARSING_ERROR;
        }

        // Add to linked list
        item->node.next = NULL;
        flist_push_back(&tx->outputs, (s_flist_node *) item);
    }

    // fee value
    if (!buffer_read_u64(buf, &tx->fee, BE)) {
        return FEE_PARSING_ERROR;
    }

    // TTL value (optional, only if includeTtl is true)
    if (tx->includeTtl) {
        if (!buffer_read_u64(buf, &tx->ttl, BE)) {
            return TO_PARSING_ERROR;  // Reuse TO_PARSING_ERROR for TTL
        }
    }

    // Validity interval start value (optional, only if includeValidityIntervalStart is true)
    if (tx->includeValidityIntervalStart) {
        if (!buffer_read_u64(buf, &tx->validityIntervalStart, BE)) {
            return TO_PARSING_ERROR;  // Reuse TO_PARSING_ERROR for VIS
        }
    }

    // Parse each mint asset group and add to linked list
    for (uint16_t ag = 0; ag < tx->num_mint_asset_groups; ag++) {
        // Allocate memory for the mint asset group list item
        mint_asset_group_list_item_t *item = (mint_asset_group_list_item_t *) app_mem_alloc(sizeof(mint_asset_group_list_item_t));
        if (item == NULL) {
            return OUTPUTS_PARSING_ERROR;  // Reuse output error for mint
        }

        // Read policy ID (no length prefix)
        uint8_t *policy_id = (uint8_t *) (buf->ptr + buf->offset);
        if (!buffer_seek_cur(buf, MINTING_POLICY_ID_SIZE)) {
            return OUTPUTS_PARSING_ERROR;
        }
        memmove(item->asset_group.policyId, policy_id, MINTING_POLICY_ID_SIZE);
        TRACE("Deserialize: Mint asset group %u: policy ID read", ag);

        // Read number of tokens (uint16, big-endian)
        if (!buffer_read_u16(buf, &item->asset_group.numTokens, BE)) {
            return OUTPUTS_PARSING_ERROR;
        }
        if (item->asset_group.numTokens > MAX_TOKENS_PER_MINT_GROUP) {
            return OUTPUTS_PARSING_ERROR;
        }
        TRACE("Deserialize: Mint asset group %u: %u tokens", ag, item->asset_group.numTokens);

        // Allocate tokens array
        item->asset_group.tokens = (mint_token_t *) app_mem_alloc(item->asset_group.numTokens * sizeof(mint_token_t));
        if (item->asset_group.tokens == NULL) {
            return OUTPUTS_PARSING_ERROR;
        }

        // Parse each token
        for (uint16_t tk = 0; tk < item->asset_group.numTokens; tk++) {
            mint_token_t *token = &item->asset_group.tokens[tk];

            // Read asset name length (uint8)
            if (!buffer_read_u8(buf, &token->assetNameLen)) {
                return OUTPUTS_PARSING_ERROR;
            }
            if (token->assetNameLen > MAX_MINT_ASSET_NAME_LEN) {
                return OUTPUTS_PARSING_ERROR;
            }

            // Read asset name (variable length)
            if (token->assetNameLen > 0) {
                uint8_t *asset_name = (uint8_t *) (buf->ptr + buf->offset);
                if (!buffer_seek_cur(buf, token->assetNameLen)) {
                    return OUTPUTS_PARSING_ERROR;
                }
                memmove(token->assetName, asset_name, token->assetNameLen);
            }

            // Read token amount (int64, BE, signed)
            if (!buffer_read_u64(buf, (uint64_t*)&token->amount, BE)) {
                return OUTPUTS_PARSING_ERROR;
            }

            TRACE("Deserialize: Mint token %u: name_len=%u, amount=", tk, token->assetNameLen);
            TRACE_INT64(token->amount);
        }

        // Add to linked list
        item->node.next = NULL;
        flist_push_back(&tx->mint_asset_groups, (s_flist_node *) item);
    }

    // Parse each withdrawal and add to linked list
    for (uint16_t i = 0; i < tx->num_withdrawals; i++) {
        // Allocate memory for the withdrawal list item
        tx_withdrawal_list_item_t *item = (tx_withdrawal_list_item_t *) app_mem_alloc(sizeof(tx_withdrawal_list_item_t));
        if (item == NULL) {
            return WITHDRAWALS_PARSING_ERROR;
        }

        // Read withdrawal amount (uint64, big-endian)
        if (!buffer_read_u64(buf, &item->withdrawal_data.amount, BE)) {
            return WITHDRAWALS_PARSING_ERROR;
        }

        // Read withdrawal credential type (uint8) and convert from wire format
        uint8_t cred_type_wire;
        if (!buffer_read_u8(buf, &cred_type_wire)) {
            return WITHDRAWALS_PARSING_ERROR;
        }

        // Convert from staking_data_source_t wire format to ext_credential_type_t
        ext_credential_type_t cred_type;
        switch (cred_type_wire) {
            case 0x22:  // STAKING_KEY_PATH
                cred_type = EXT_CREDENTIAL_KEY_PATH;
                break;
            case 0x33:  // STAKING_KEY_HASH
                cred_type = EXT_CREDENTIAL_KEY_HASH;
                break;
            case 0x55:  // STAKING_SCRIPT_HASH
                cred_type = EXT_CREDENTIAL_SCRIPT_HASH;
                break;
            default:
                return WITHDRAWALS_PARSING_ERROR;
        }
        item->withdrawal_data.stakeCredential.type = cred_type;

        // Read withdrawal credential based on type
        switch (cred_type) {
            case EXT_CREDENTIAL_KEY_PATH: {
                // Read withdrawal key path using bip44 wire format
                if (!buffer_read_bip44_path(buf, &item->withdrawal_data.stakeCredential.keyPath)) {
                    return WITHDRAWALS_PARSING_ERROR;
                }
                TRACE("Deserialize: Withdrawal %u key path, length=%u", i, item->withdrawal_data.stakeCredential.keyPath.length);
                break;
            }
            case EXT_CREDENTIAL_KEY_HASH: {
                // Read withdrawal key hash (fixed 28 bytes, no length prefix)
                uint8_t *hash_ptr = (uint8_t *) (buf->ptr + buf->offset);
                if (!buffer_seek_cur(buf, ADDRESS_KEY_HASH_LENGTH)) {
                    return WITHDRAWALS_PARSING_ERROR;
                }
                memmove(item->withdrawal_data.stakeCredential.keyHash, hash_ptr, ADDRESS_KEY_HASH_LENGTH);
                TRACE("Deserialize: Withdrawal %u key hash", i);
                break;
            }
            case EXT_CREDENTIAL_SCRIPT_HASH: {
                // Read withdrawal script hash (fixed 28 bytes, no length prefix)
                uint8_t *hash_ptr = (uint8_t *) (buf->ptr + buf->offset);
                if (!buffer_seek_cur(buf, SCRIPT_HASH_LENGTH)) {
                    return WITHDRAWALS_PARSING_ERROR;
                }
                memmove(item->withdrawal_data.stakeCredential.scriptHash, hash_ptr, SCRIPT_HASH_LENGTH);
                TRACE("Deserialize: Withdrawal %u script hash", i);
                break;
            }
            default:
                return WITHDRAWALS_PARSING_ERROR;
        }

        // Initialize previousRewardAccount to zeros (will be filled during hash building)
        explicit_bzero(item->withdrawal_data.previousRewardAccount, REWARD_ACCOUNT_SIZE);

        // Add to linked list
        item->node.next = NULL;
        flist_push_back(&tx->withdrawals, (s_flist_node *) item);
    }

    return (buf->offset == buf->size) ? PARSING_OK : WRONG_LENGTH_ERROR;
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

        // Free inline datum data
        if (item->output_data.datum.type == DATUM_INLINE &&
            item->output_data.datum.inline_data.data != NULL) {
            app_mem_free(item->output_data.datum.inline_data.data);
        }

        // Free reference script data
        if (item->output_data.hasRefScript && item->output_data.refScript.data != NULL) {
            app_mem_free(item->output_data.refScript.data);
        }

        // Free the list item itself
        app_mem_free(output_node);
        output_node = next;
    }
    tx->outputs = NULL;
}

/// Clean up dynamically allocated memory in transaction withdrawals (including list items)
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

/**
 * Cleanup transaction lists by freeing all allocated memory
 */
void tx_context_cleanup(void) {
    transaction_t *tx = &G_context.tx_info.transaction;

    // Free all input items from the linked list
    s_flist_node *input_node = tx->inputs;
    while (input_node != NULL) {
        s_flist_node *next = input_node->next;
        app_mem_free(input_node);
        input_node = next;
    }
    tx->inputs = NULL;

    // Free all output items with their associated data (asset groups, datums, ref scripts)
    transaction_free_outputs(tx);

    // Free all mint asset groups with their tokens
    transaction_free_mint(tx);

    // Free all withdrawal items
    transaction_free_withdrawals(tx);

    // Free raw tx buffer
    if (G_context.tx_info.raw_tx != NULL) {
        app_mem_free(G_context.tx_info.raw_tx);
        G_context.tx_info.raw_tx = NULL;
    }
}
