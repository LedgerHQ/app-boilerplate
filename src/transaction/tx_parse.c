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
#include "transaction/tx.h"
#include "utils.h"
#include "utils/assert.h"
#include "utils/textUtils.h"
#include "transaction/tx_constants.h"
#include "tx_output_types.h"
#include "globals.h"
static uint16_t _map_parser_status_to_swo(parser_status_e status);

static parser_status_e parse_tx_inputs(buffer_t *buf, transaction_t *tx);
static parser_status_e parse_tx_outputs(buffer_t *buf, transaction_t *tx);
static parser_status_e parse_tx_mint_groups(buffer_t *buf, transaction_t *tx);
static parser_status_e parse_tx_certificates(buffer_t *buf, transaction_t *tx);
static parser_status_e parse_tx_withdrawals(buffer_t *buf, transaction_t *tx);

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

    status = parse_tx_inputs(buf, tx);
    if (status != PARSING_OK) {
        return status;
    }

    status = parse_tx_outputs(buf, tx);
    if (status != PARSING_OK) {
        return status;
    }

    if (!buffer_read_u64(buf, &tx->fee, BE)) {
        return FEE_PARSING_ERROR;
    }

    if (tx->includeTtl) {
        if (!buffer_read_u64(buf, &tx->ttl, BE)) {
            return TTL_PARSING_ERROR;
        }
    }

    status = parse_tx_certificates(buf, tx);
    if (status != PARSING_OK) {
        return status;
    }

    if (tx->includeValidityIntervalStart) {
        if (!buffer_read_u64(buf, &tx->validityIntervalStart, BE)) {
            return VALIDITY_INTERVAL_START_PARSING_ERROR;
        }
    }

    status = parse_tx_mint_groups(buf, tx);
    if (status != PARSING_OK) {
        return status;
    }

    status = parse_tx_withdrawals(buf, tx);
    if (status != PARSING_OK) {
        return status;
    }

    if (buf->offset != buf->size) {
        TRACE("TX parsing: bytes remaining=%u vs expected=%u", (uint32_t)(buf->size - buf->offset), (uint32_t)buf->size);
        return TX_BUFFER_NOT_FULLY_CONSUMED_ERROR;
    }
    return PARSING_OK;
}

static uint16_t _map_parser_status_to_swo(parser_status_e status) {
    switch (status) {
        case INPUTS_PARSING_ERROR:
        case INPUTS_COUNT_PARSING_ERROR:
            return SWO_TX_PARSING_FAIL_INPUTS;
        case OUTPUTS_PARSING_ERROR:
        case OUTPUTS_COUNT_PARSING_ERROR:
        case OUTPUT_DESTINATION_TYPE_ERROR:
        case OUTPUT_ADDRESS_SIZE_ERROR:
        case WITHDRAWALS_PARSING_ERROR:
            return SWO_TX_PARSING_FAIL_OUTPUTS;
        case CERTIFICATES_PARSING_ERROR:
            return SWO_TX_PARSING_FAIL_CERTIFICATES;
        case FEE_PARSING_ERROR:
            return SWO_TX_PARSING_FAIL_FEE;
        case TTL_PARSING_ERROR:
            return SWO_TX_PARSING_FAIL_TTL;
        case VALIDITY_INTERVAL_START_PARSING_ERROR:
            return SWO_TX_PARSING_FAIL_VALIDITY_INTERVAL_START;
        case TX_SIZE_TOO_LARGE_ERROR:
            return SWO_INVALID_TX_LENGTH;
        case TX_BUFFER_NOT_FULLY_CONSUMED_ERROR:
            return SWO_TX_PARSING_FAIL_BUFFER_NOT_FULLY_CONSUMED;
        default:
            return SWO_TX_PARSING_FAIL;
    }
}

int tx_handle_parse_error(parser_status_e status) {
    LEDGER_ASSERT(status != PARSING_OK, "tx_parse received PARSING_OK");
    tx_context_cleanup();
    uint16_t swo = _map_parser_status_to_swo(status);
    TRACE("tx_handle_parse_error status=%d swo=0x%04x", status, swo);
    return send_error_and_reset(swo);
}

static parser_status_e parse_tx_inputs(buffer_t *buf, transaction_t *tx) {
    for (uint16_t i = 0; i < tx->num_inputs; i++) {
        tx_input_list_item_t *item = (tx_input_list_item_t *) app_mem_alloc(sizeof(tx_input_list_item_t));
        if (item == NULL) {
            return INPUTS_PARSING_ERROR;
        }

        STATIC_ASSERT(SIZEOF(item->input_data.txHashBuffer) == TX_HASH_LENGTH,
                      "tx input hash buffer size mismatch");
        if (!buffer_read_bytes(buf, item->input_data.txHashBuffer, TX_HASH_LENGTH)) {
            return INPUTS_PARSING_ERROR;
        }

        if (!buffer_read_u32(buf, &item->input_data.index, BE)) {
            return INPUTS_PARSING_ERROR;
        }

        item->node.next = NULL;
        flist_push_back(&tx->inputs, (s_flist_node *) item);
    }
    return PARSING_OK;
}

static parser_status_e parse_tx_outputs(buffer_t *buf, transaction_t *tx) {
    for (uint16_t i = 0; i < tx->num_outputs; i++) {
        uint16_t output_len;
        if (!buffer_read_u16(buf, &output_len, BE)) {
            return OUTPUTS_PARSING_ERROR;
        }
        size_t output_start = buf->offset;

        tx_output_list_item_t *item = (tx_output_list_item_t *) app_mem_alloc(sizeof(tx_output_list_item_t));
        if (item == NULL) {
            return OUTPUTS_PARSING_ERROR;
        }

        uint8_t dest_type;
        if (!buffer_read_u8(buf, &dest_type)) {
            return OUTPUTS_PARSING_ERROR;
        }
        TRACE("Deserialize: Output %u destination type=0x%02x (1=THIRD_PARTY, 2=DEVICE_OWNED)", i, dest_type);
        item->output_data.destination.type = (tx_output_destination_type_t) dest_type;

        if (dest_type == DESTINATION_THIRD_PARTY) {
            uint16_t addr_size;
            if (!buffer_read_u16(buf, &addr_size, BE)) {
                return OUTPUTS_PARSING_ERROR;
            }
            if (addr_size == 0 || addr_size > MAX_ADDRESS_LENGTH) {
                return OUTPUT_ADDRESS_SIZE_ERROR;
            }
            item->output_data.destination.address.size = addr_size;

            STATIC_ASSERT(SIZEOF(item->output_data.destination.address.buffer) == MAX_ADDRESS_LENGTH,
                          "destination address buffer size mismatch");
            if (!buffer_read_bytes(buf, item->output_data.destination.address.buffer, addr_size)) {
                return OUTPUTS_PARSING_ERROR;
            }

        } else if (dest_type == DESTINATION_DEVICE_OWNED) {
            uint8_t addr_type;
            if (!buffer_read_u8(buf, &addr_type)) {
                return OUTPUTS_PARSING_ERROR;
            }
            item->output_data.destination.params.type = (address_type_t) addr_type;

            if (addr_type == BYRON) {
                uint32_t protocol_magic;
                if (!buffer_read_u32(buf, &protocol_magic, BE)) {
                    return OUTPUTS_PARSING_ERROR;
                }
                item->output_data.destination.params.protocolMagic = protocol_magic;
            } else {
                item->output_data.destination.params.networkId = tx->networkId;
            }

            payment_choice_t payment_choice = determinePaymentChoice(item->output_data.destination.params.type);
            switch (payment_choice) {
                case PAYMENT_PATH:
                    if (!buffer_read_bip44_path(buf, &item->output_data.destination.params.paymentKeyPath)) {
                        return OUTPUTS_PARSING_ERROR;
                    }
                    TRACE("Deserialize: Payment path, length=%u", item->output_data.destination.params.paymentKeyPath.length);
                    break;
                case PAYMENT_SCRIPT_HASH: {
                    STATIC_ASSERT(SIZEOF(item->output_data.destination.params.paymentScriptHash) == SCRIPT_HASH_LENGTH,
                                  "payment script hash size mismatch");
                    if (!buffer_read_bytes(buf,
                                           item->output_data.destination.params.paymentScriptHash,
                                           SCRIPT_HASH_LENGTH)) {
                        return OUTPUTS_PARSING_ERROR;
                    }
                    TRACE("Deserialize: Payment script hash");
                    break;
                }
                case PAYMENT_NONE:
                    return OUTPUTS_PARSING_ERROR;
            }

            uint8_t staking_choice;
            if (!buffer_read_u8(buf, &staking_choice)) {
                return OUTPUTS_PARSING_ERROR;
            }
            item->output_data.destination.params.stakingDataSource = (staking_data_source_t) staking_choice;

            switch (staking_choice) {
                case STAKING_KEY_PATH:
                    if (!buffer_read_bip44_path(buf, &item->output_data.destination.params.stakingKeyPath)) {
                        return OUTPUTS_PARSING_ERROR;
                    }
                    break;
                case STAKING_KEY_HASH: {
                    STATIC_ASSERT(SIZEOF(item->output_data.destination.params.stakingKeyHash) == ADDRESS_KEY_HASH_LENGTH,
                                  "staking key hash size mismatch");
                    if (!buffer_read_bytes(buf,
                                           item->output_data.destination.params.stakingKeyHash,
                                           ADDRESS_KEY_HASH_LENGTH)) {
                        return OUTPUTS_PARSING_ERROR;
                    }
                    break;
                }
                case STAKING_SCRIPT_HASH: {
                    STATIC_ASSERT(SIZEOF(item->output_data.destination.params.stakingScriptHash) == SCRIPT_HASH_LENGTH,
                                  "staking script hash size mismatch");
                    if (!buffer_read_bytes(buf,
                                           item->output_data.destination.params.stakingScriptHash,
                                           SCRIPT_HASH_LENGTH)) {
                        return OUTPUTS_PARSING_ERROR;
                    }
                    break;
                }
                case BLOCKCHAIN_POINTER: {
                    STATIC_ASSERT(SIZEOF(item->output_data.destination.params.stakingKeyBlockchainPointer) == sizeof(blockchainPointer_t),
                                  "staking blockchain pointer size mismatch");
                    if (!buffer_read_bytes(
                            buf,
                            (uint8_t *) &item->output_data.destination.params.stakingKeyBlockchainPointer,
                            sizeof(blockchainPointer_t))) {
                        return OUTPUTS_PARSING_ERROR;
                    }
                    break;
                }
                case NO_STAKING:
                    break;
                default:
                    return OUTPUTS_PARSING_ERROR;
            }

        } else {
            return OUTPUT_DESTINATION_TYPE_ERROR;
        }

        if (!buffer_read_u64(buf, &item->output_data.adaAmount, BE)) {
            return OUTPUTS_PARSING_ERROR;
        }

        uint8_t output_format;
        if (!buffer_read_u8(buf, &output_format)) {
            return OUTPUTS_PARSING_ERROR;
        }
        if (output_format != ARRAY_LEGACY && output_format != MAP_BABBAGE) {
            return OUTPUTS_PARSING_ERROR;
        }
        item->output_data.format = (tx_output_serialization_format_t) output_format;
        TRACE("Deserialize: Output %u format=%u", i, output_format);

        if (!buffer_read_u16(buf, &item->output_data.numAssetGroups, BE)) {
            return OUTPUTS_PARSING_ERROR;
        }
        TRACE("Deserialize: Output %u: %u asset groups", i, item->output_data.numAssetGroups);

        if (item->output_data.numAssetGroups > MAX_ASSET_GROUPS_PER_OUTPUT) {
            return OUTPUTS_PARSING_ERROR;
        }

        if (item->output_data.numAssetGroups > 0) {
            item->output_data.assetGroups =
                (asset_group_t *) app_mem_alloc(item->output_data.numAssetGroups * sizeof(asset_group_t));
            if (item->output_data.assetGroups == NULL) {
                return OUTPUTS_PARSING_ERROR;
            }

            for (uint16_t ag = 0; ag < item->output_data.numAssetGroups; ag++) {
                asset_group_t *group = &item->output_data.assetGroups[ag];
                STATIC_ASSERT(SIZEOF(group->policyId) == MINTING_POLICY_ID_LENGTH,
                              "asset group policy size mismatch");
                if (!buffer_read_bytes(buf, group->policyId, MINTING_POLICY_ID_LENGTH)) {
                    return OUTPUTS_PARSING_ERROR;
                }
                TRACE("Deserialize: Asset group %u: policy ID read", ag);

                if (!buffer_read_u16(buf, &group->numTokens, BE)) {
                    return OUTPUTS_PARSING_ERROR;
                }
                if (group->numTokens > MAX_TOKENS_PER_ASSET_GROUP) {
                    return OUTPUTS_PARSING_ERROR;
                }
                TRACE("Deserialize: Asset group %u: %u tokens", ag, group->numTokens);

                group->tokens = (output_token_t *) app_mem_alloc(group->numTokens * sizeof(output_token_t));
                if (group->tokens == NULL) {
                    return OUTPUTS_PARSING_ERROR;
                }

                for (uint16_t tk = 0; tk < group->numTokens; tk++) {
                    output_token_t *token = &group->tokens[tk];
                    if (!buffer_read_u8(buf, &token->assetNameLen)) {
                        return OUTPUTS_PARSING_ERROR;
                    }
                    if (token->assetNameLen > ASSET_NAME_DISPLAY_SIZE) {
                        return OUTPUTS_PARSING_ERROR;
                    }

                    if (token->assetNameLen > 0) {
                        STATIC_ASSERT(SIZEOF(token->assetName) == ASSET_NAME_DISPLAY_SIZE,
                                      "output token asset buffer size mismatch");
                        if (!buffer_read_bytes(buf, token->assetName, token->assetNameLen)) {
                            return OUTPUTS_PARSING_ERROR;
                        }
                    }

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

        uint8_t datum_wire_type;
        if (!buffer_read_u8(buf, &datum_wire_type)) {
            return OUTPUTS_PARSING_ERROR;
        }
        TRACE("Deserialize: Output %u: Datum wire type=%u", i, datum_wire_type);

        if (datum_wire_type == 0) {
            item->output_data.datum.type = (datum_type_t) 0xFF;
        } else if (datum_wire_type == 1) {
            item->output_data.datum.type = DATUM_HASH;
        } else if (datum_wire_type == 2) {
            item->output_data.datum.type = DATUM_INLINE;
        } else {
            return OUTPUTS_PARSING_ERROR;
        }

        switch (datum_wire_type) {
            case 0:
                break;
            case 1: {
                STATIC_ASSERT(SIZEOF(item->output_data.datum.hash) == OUTPUT_DATUM_HASH_LENGTH,
                              "datum hash size mismatch");
                if (!buffer_read_bytes(buf,
                                       item->output_data.datum.hash,
                                       OUTPUT_DATUM_HASH_LENGTH)) {
                    return OUTPUTS_PARSING_ERROR;
                }
                TRACE("Deserialize: Datum hash read");
                break;
            }
            case 2: {
                uint16_t datum_size;
                if (!buffer_read_u16(buf, &datum_size, BE)) {
                    return OUTPUTS_PARSING_ERROR;
                }
                if (datum_size > MAX_DATUM_INLINE_LENGTH) {
                    return OUTPUTS_PARSING_ERROR;
                }
                item->output_data.datum.inline_data.size = datum_size;

                item->output_data.datum.inline_data.data = (uint8_t *) app_mem_alloc(datum_size);
                if (item->output_data.datum.inline_data.data == NULL) {
                    return OUTPUTS_PARSING_ERROR;
                }

                if (!buffer_read_bytes(buf,
                                       item->output_data.datum.inline_data.data,
                                       datum_size)) {
                    return OUTPUTS_PARSING_ERROR;
                }
                TRACE("Deserialize: Inline datum read: %u bytes", datum_size);
                break;
            }
            default:
                return OUTPUTS_PARSING_ERROR;
        }

        uint8_t has_ref_script;
        if (!buffer_read_u8(buf, &has_ref_script)) {
            return OUTPUTS_PARSING_ERROR;
        }
        item->output_data.hasRefScript = (has_ref_script != 0);
        TRACE("Deserialize: Output %u: Has reference script=%u", i, has_ref_script);

        if (item->output_data.hasRefScript) {
            uint16_t script_size;
            if (!buffer_read_u16(buf, &script_size, BE)) {
                return OUTPUTS_PARSING_ERROR;
            }
            if (script_size > MAX_REF_SCRIPT_LENGTH) {
                return OUTPUTS_PARSING_ERROR;
            }
            item->output_data.refScript.size = script_size;

            item->output_data.refScript.data = (uint8_t *) app_mem_alloc(script_size);
            if (item->output_data.refScript.data == NULL) {
                return OUTPUTS_PARSING_ERROR;
            }

            if (!buffer_read_bytes(buf, item->output_data.refScript.data, script_size)) {
                return OUTPUTS_PARSING_ERROR;
            }
            TRACE("Deserialize: Reference script read: %u bytes", script_size);
        }

        if (buf->offset - output_start != output_len) {
            TRACE("Deserialize: Output length mismatch: expected=%u, consumed=%u",
                  output_len, (uint32_t)(buf->offset - output_start));
            return OUTPUTS_PARSING_ERROR;
        }

        item->node.next = NULL;
        flist_push_back(&tx->outputs, (s_flist_node *) item);
    }
    return PARSING_OK;
}

static parser_status_e parse_tx_mint_groups(buffer_t *buf, transaction_t *tx) {
    for (uint16_t ag = 0; ag < tx->num_mint_asset_groups; ag++) {
        mint_asset_group_list_item_t *item = (mint_asset_group_list_item_t *) app_mem_alloc(sizeof(mint_asset_group_list_item_t));
        if (item == NULL) {
            return OUTPUTS_PARSING_ERROR;
        }

        STATIC_ASSERT(SIZEOF(item->asset_group.policyId) == MINTING_POLICY_ID_LENGTH,
                      "mint asset group policy size mismatch");
        if (!buffer_read_bytes(buf, item->asset_group.policyId, MINTING_POLICY_ID_LENGTH)) {
            return OUTPUTS_PARSING_ERROR;
        }
        TRACE("Deserialize: Mint asset group %u: policy ID read", ag);

        if (!buffer_read_u16(buf, &item->asset_group.numTokens, BE)) {
            return OUTPUTS_PARSING_ERROR;
        }
        if (item->asset_group.numTokens > MAX_TOKENS_PER_MINT_GROUP) {
            return OUTPUTS_PARSING_ERROR;
        }
        TRACE("Deserialize: Mint asset group %u: %u tokens", ag, item->asset_group.numTokens);

        item->asset_group.tokens = (mint_token_t *) app_mem_alloc(item->asset_group.numTokens * sizeof(mint_token_t));
        if (item->asset_group.tokens == NULL) {
            return OUTPUTS_PARSING_ERROR;
        }

        for (uint16_t tk = 0; tk < item->asset_group.numTokens; tk++) {
            mint_token_t *token = &item->asset_group.tokens[tk];
            if (!buffer_read_u8(buf, &token->assetNameLen)) {
                return OUTPUTS_PARSING_ERROR;
            }
            if (token->assetNameLen > MAX_MINT_ASSET_NAME_LENGTH) {
                return OUTPUTS_PARSING_ERROR;
            }

            if (token->assetNameLen > 0) {
                STATIC_ASSERT(SIZEOF(token->assetName) == MAX_MINT_ASSET_NAME_LENGTH,
                              "mint token asset buffer size mismatch");
                if (!buffer_read_bytes(buf, token->assetName, token->assetNameLen)) {
                    return OUTPUTS_PARSING_ERROR;
                }
            }

            if (!buffer_read_u64(buf, (uint64_t*)&token->amount, BE)) {
                return OUTPUTS_PARSING_ERROR;
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
    for (uint16_t i = 0; i < tx->num_certificates; i++) {
        tx_certificate_list_item_t *item = (tx_certificate_list_item_t *) app_mem_alloc(sizeof(tx_certificate_list_item_t));
        if (item == NULL) {
            return CERTIFICATES_PARSING_ERROR;
        }

        // Read certificate type
        uint8_t cert_type_wire;
        if (!buffer_read_u8(buf, &cert_type_wire)) {
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
            return WITHDRAWALS_PARSING_ERROR;
        }

        if (!buffer_read_u64(buf, &item->withdrawal_data.amount, BE)) {
            return WITHDRAWALS_PARSING_ERROR;
        }

        uint8_t cred_type_wire;
        if (!buffer_read_u8(buf, &cred_type_wire)) {
            return WITHDRAWALS_PARSING_ERROR;
        }

        ext_credential_type_t cred_type;
        switch (cred_type_wire) {
            case 0x22:
                cred_type = EXT_CREDENTIAL_KEY_PATH;
                break;
            case 0x33:
                cred_type = EXT_CREDENTIAL_KEY_HASH;
                break;
            case 0x55:
                cred_type = EXT_CREDENTIAL_SCRIPT_HASH;
                break;
            default:
                return WITHDRAWALS_PARSING_ERROR;
        }
        item->withdrawal_data.stakeCredential.type = cred_type;

        switch (cred_type) {
            case EXT_CREDENTIAL_KEY_PATH:
                if (!buffer_read_bip44_path(buf, &item->withdrawal_data.stakeCredential.keyPath)) {
                    return WITHDRAWALS_PARSING_ERROR;
                }
                TRACE("Deserialize: Withdrawal %u key path, length=%u", i, item->withdrawal_data.stakeCredential.keyPath.length);
                break;
            case EXT_CREDENTIAL_KEY_HASH: {
                STATIC_ASSERT(SIZEOF(item->withdrawal_data.stakeCredential.keyHash) == ADDRESS_KEY_HASH_LENGTH,
                              "credential key hash size mismatch");
                if (!buffer_read_bytes(buf,
                                       item->withdrawal_data.stakeCredential.keyHash,
                                       ADDRESS_KEY_HASH_LENGTH)) {
                    return WITHDRAWALS_PARSING_ERROR;
                }
                TRACE("Deserialize: Withdrawal %u key hash", i);
                break;
            }
            case EXT_CREDENTIAL_SCRIPT_HASH: {
                STATIC_ASSERT(SIZEOF(item->withdrawal_data.stakeCredential.scriptHash) == SCRIPT_HASH_LENGTH,
                              "credential script hash size mismatch");
                if (!buffer_read_bytes(buf,
                                       item->withdrawal_data.stakeCredential.scriptHash,
                                       SCRIPT_HASH_LENGTH)) {
                    return WITHDRAWALS_PARSING_ERROR;
                }
                TRACE("Deserialize: Withdrawal %u script hash", i);
                break;
            }
            default:
                return WITHDRAWALS_PARSING_ERROR;
        }

        explicit_bzero(item->withdrawal_data.previousRewardAccount, REWARD_ACCOUNT_LENGTH);
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

    // Free all certificate items
    transaction_free_certificates(tx);

    // Free all mint asset groups with their tokens
    transaction_free_mint(tx);

    // Free all withdrawal items
    transaction_free_withdrawals(tx);

    // Free raw tx buffer
    if (G_context.tx_info.raw_tx != NULL) {
        app_mem_free(G_context.tx_info.raw_tx);
        G_context.tx_info.raw_tx = NULL;
    }
    G_context.tx_info.planned_ui_pairs = 0;
}
