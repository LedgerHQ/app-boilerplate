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

#include <stdint.h>   // uint*_t
#include <stdbool.h>  // bool
#include <stddef.h>   // size_t
#include <string.h>   // memset, explicit_bzero

#include "os.h"
#include "buffer.h"
#include "nbgl_use_case.h"
#include "utils/buffer_utils.h"

#include "sign_tx.h"
#include "cardano_swo.h"
#include "globals.h"
#include "display.h"
#include "transaction/tx.h"
#include "transaction/tx_aux_data_types.h"
#include "tx_output_types.h"
#include "tx_parse.h"
#include "memory/mem.h"
#include "utils/utils.h"
#include "utils/cardano_os_utils.h"
#include "utils/cbor.h"
#include "transaction/tx_hash_builder.h"
#include "messageSigning.h"
#include "securityPolicy/securityPolicy.h"
#include "dispatcher.h"
#include "addressUtils/bip44.h"
#include "addressUtils/addressUtilsShelley.h"
#include "transaction/tx_utils.h"
#include "ui/menu.h"
#include "transaction/tx_prepare.h"

/**
 * Helper: Initialize transaction from P1_TX_INIT APDU
 * Validates all transaction metadata and checks security policy
 */
static int handle_tx_init_apdu(buffer_t *cdata) {
    G_context.tx_info.raw_tx = NULL;
    G_context.tx_info.raw_tx_len = 0;
    warning_bits_init(&G_context.tx_info.warning_bits);
    G_context.tx_info.planned_ui_pairs = 0;

    // Read and validate options (fixed header)
    uint64_t options;
    if (!buffer_read_u64(cdata, &options, BE)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }
    bool tagCborSets = options & TX_OPTIONS_TAG_CBOR_SETS;
    options &= ~TX_OPTIONS_TAG_CBOR_SETS;
    if (options != 0) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }
    G_context.tx_info.transaction.tagCborSets = tagCborSets;

    // Read network parameters and signing mode
    if (!buffer_read_u8(cdata, &G_context.tx_info.transaction.networkId) ||
        !buffer_read_u32(cdata, &G_context.tx_info.transaction.protocolMagic, BE)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }

    // Validate network ID immediately - return specific error code
    if (!isValidNetworkId(G_context.tx_info.transaction.networkId)) {
        return send_error_and_reset(SWO_INVALID_NETWORK_ID);
    }

    // Validate mainnet protocol magic - return specific error code
    if (G_context.tx_info.transaction.networkId == MAINNET_NETWORK_ID &&
        G_context.tx_info.transaction.protocolMagic != MAINNET_PROTOCOL_MAGIC) {
        return send_error_and_reset(SWO_INVALID_PROTOCOL_MAGIC);
    }

    uint8_t txSigningMode;
    if (!buffer_read_u8(cdata, &txSigningMode)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }
    G_context.tx_info.transaction.txSigningMode = (sign_tx_signingmode_t) txSigningMode;

    // Read transaction structure counts (fields 0-1: inputs and outputs, always present)
    if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_inputs, BE) ||
        !buffer_read_u16(cdata, &G_context.tx_info.transaction.num_outputs, BE)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }

    // Field 3 (TTL) - optional
    uint8_t includeTtlByte;
    if (!buffer_read_u8(cdata, &includeTtlByte)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeTtlByte, &G_context.tx_info.transaction.includeTtl)) {
        return send_error_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
    }

    // Field 4 (certificates) - optional
    if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_certificates, BE)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }
    TRACE(">>>INIT: num_certificates=%u", G_context.tx_info.transaction.num_certificates);

    // Field 5 (withdrawals) - optional
    if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_withdrawals, BE)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }

    // Field 7 (auxiliary data hash) - optional
    uint8_t includeAuxDataHashByte;
    bool includeAuxDataHash = false;
    if (!buffer_read_u8(cdata, &includeAuxDataHashByte)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeAuxDataHashByte, &includeAuxDataHash)) {
        return send_error_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
    }
    G_context.tx_info.transaction.includeAuxDataHash = includeAuxDataHash;
    G_context.tx_info.transaction.auxDataType = AUX_DATA_TYPE_ARBITRARY_HASH;
    if (includeAuxDataHash) {
        if (!buffer_read_bytes(cdata,
                               G_context.tx_info.transaction.auxDataHash,
                               AUX_DATA_HASH_LENGTH)) {
            return send_error_and_reset(SWO_TX_PARSING_FAIL);
        }
    } else {
        explicit_bzero(G_context.tx_info.transaction.auxDataHash,
                       AUX_DATA_HASH_LENGTH);
    }

    // Field 8 (validity interval start) - optional
    uint8_t includeValidityIntervalStartByte;
    if (!buffer_read_u8(cdata, &includeValidityIntervalStartByte)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeValidityIntervalStartByte, &G_context.tx_info.transaction.includeValidityIntervalStart)) {
        return send_error_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
    }

    // Field 9 (mint) - optional
    if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_mint_asset_groups, BE)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }

    // Field 11 (script data hash) - optional
    uint8_t includeScriptDataHashByte;
    bool includeScriptDataHash = false;
    if (!buffer_read_u8(cdata, &includeScriptDataHashByte)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeScriptDataHashByte, &includeScriptDataHash)) {
        return send_error_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
    }
    G_context.tx_info.transaction.includeScriptDataHash = includeScriptDataHash;

    // Field 13 (collateral inputs)
    if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_collateral_inputs, BE)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }

    // Field 14 (required signers)
    if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_required_signers, BE)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }

    // Field 15 (network ID)
    uint8_t includeNetworkIdByte;
    if (!buffer_read_u8(cdata, &includeNetworkIdByte)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeNetworkIdByte, &G_context.tx_info.transaction.includeNetworkId)) {
        return send_error_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
    }

    // Field 16 (collateral output)
    uint8_t includeCollateralOutputByte;
    if (!buffer_read_u8(cdata, &includeCollateralOutputByte)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeCollateralOutputByte, &G_context.tx_info.transaction.includeCollateralOutput)) {
        return send_error_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
    }

    // Field 17 (total collateral)
    uint8_t includeTotalCollateralByte;
    if (!buffer_read_u8(cdata, &includeTotalCollateralByte)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeTotalCollateralByte, &G_context.tx_info.transaction.includeTotalCollateral)) {
        return send_error_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
    }

    // Field 18 (reference inputs)
    if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_reference_inputs, BE)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }

    // Field 19 (voting procedures)
    if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_voters, BE)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }

    // Field 21 (treasury) - optional
    uint8_t includeTreasuryByte;
    if (!buffer_read_u8(cdata, &includeTreasuryByte)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeTreasuryByte, &G_context.tx_info.transaction.includeTreasury)) {
        return send_error_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
    }

    // Field 22 (donation) - optional
    uint8_t includeDonationByte;
    if (!buffer_read_u8(cdata, &includeDonationByte)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeDonationByte, &G_context.tx_info.transaction.includeDonation)) {
        return send_error_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
    }

    // Read number of witnesses
    if (!buffer_read_u16(cdata, &G_context.tx_info.num_witnesses, BE)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }

    TRACE("TX Mode=%d, Network: ID=%d, Magic=%d, Inputs=%d, Outputs=%d, Withdrawals=%d, Mint=%d, TTL=%d, VIS=%d, Witnesses=%d",
        G_context.tx_info.transaction.txSigningMode,
        G_context.tx_info.transaction.networkId,
        G_context.tx_info.transaction.protocolMagic,
        G_context.tx_info.transaction.num_inputs,
        G_context.tx_info.transaction.num_outputs,
        G_context.tx_info.transaction.num_withdrawals,
        G_context.tx_info.transaction.num_mint_asset_groups,
        G_context.tx_info.transaction.includeTtl,
        G_context.tx_info.transaction.includeValidityIntervalStart,
        G_context.tx_info.num_witnesses
    );

    // Check security policy
    bool includeMint = (G_context.tx_info.transaction.num_mint_asset_groups > 0);

    security_policy_t init_policy = policyForSignTxInit(
        G_context.tx_info.transaction.txSigningMode,
        G_context.tx_info.transaction.networkId,
        G_context.tx_info.transaction.protocolMagic,
        G_context.tx_info.transaction.num_outputs,
        G_context.tx_info.transaction.num_certificates,
        G_context.tx_info.transaction.num_withdrawals,
        includeMint,
        G_context.tx_info.transaction.includeScriptDataHash,
        G_context.tx_info.transaction.num_collateral_inputs,
        G_context.tx_info.transaction.num_required_signers,
        G_context.tx_info.transaction.includeNetworkId,
        G_context.tx_info.transaction.includeCollateralOutput,
        G_context.tx_info.transaction.includeTotalCollateral,
        G_context.tx_info.transaction.num_reference_inputs,
        G_context.tx_info.transaction.num_voters,
        G_context.tx_info.transaction.includeTreasury,
        G_context.tx_info.transaction.includeDonation,
        &G_context.tx_info.warning_bits);

    TRACE("Transaction init security policy: %d", (int) init_policy);

    if (init_policy == POLICY_DENY) {
        TRACE("Security policy DENY - rejecting transaction init");
        return send_error_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
    }

    // Show spinner to indicate transaction data is being processed
    TRACE("Calling nbgl_useCaseSpinner(\"Processing\")");
    nbgl_useCaseSpinner("Processing");

    // Transition to CHUNKS state - now ready to receive transaction data chunks
    G_context.state.tx_state = TX_STATE_CHUNKS;
    TRACE("Transaction initialized, waiting for data chunks");

    return io_send_sw(SWO_SUCCESS);
}

/**
 * Helper: Accumulate transaction data chunks into buffer
 * Returns SWO_SUCCESS if more chunks expected, or falls through to parse if final chunk
 */
static int handle_tx_data_chunk(buffer_t *cdata, bool more) {
    TRACE("SWO_SUCCESS constant = 0x%04x", SWO_SUCCESS);
    // Validate we're in the correct state for receiving chunks
    if (G_context.state.tx_state != TX_STATE_CHUNKS) {
        TRACE("Invalid state for chunk reception: expected TX_STATE_CHUNKS, got %d", G_context.state.tx_state);
        return send_error_and_reset(SWO_BAD_STATE);
    }

    // Allocate buffer on first data chunk
    if (G_context.tx_info.raw_tx == NULL) {
        TRACE("Allocating transaction buffer: %d bytes", TX_BUFFER_SIZE);
        app_mem_dump_stats();
        G_context.tx_info.raw_tx = (uint8_t *) app_mem_alloc(TX_BUFFER_SIZE);
        if (G_context.tx_info.raw_tx == NULL) {
            TRACE("Failed to allocate %d byte transaction buffer!", TX_BUFFER_SIZE);
            app_mem_dump_stats();
            return send_error_and_reset(SWO_INSUFFICIENT_MEMORY);
        }
        TRACE("Transaction buffer allocated: %d bytes at %p", TX_BUFFER_SIZE, G_context.tx_info.raw_tx);
    }

    // Check if adding this chunk would exceed buffer
    if (G_context.tx_info.raw_tx_len + cdata->size > TX_BUFFER_SIZE) {
        TRACE("Transaction too large: current=%d, chunk=%d, max=%d",
              G_context.tx_info.raw_tx_len, cdata->size, TX_BUFFER_SIZE);
        return send_error_and_reset(SWO_INVALID_TX_LENGTH);
    }

    // Copy chunk data
    if (!buffer_move(cdata,
                     G_context.tx_info.raw_tx + G_context.tx_info.raw_tx_len,
                     cdata->size)) {
        TRACE("Failed to copy transaction chunk");
        return send_error_and_reset(SWO_TX_PARSING_FAIL);
    }
    G_context.tx_info.raw_tx_len += cdata->size;
    TRACE("Copied %d bytes, total: %d", cdata->size, G_context.tx_info.raw_tx_len);

    if (more) {
        return io_send_sw(SWO_SUCCESS);
    }

    // Final chunk - will be handled by caller
    return SWO_SUCCESS;
}

int handler_sign_tx(buffer_t *cdata, uint8_t chunk_type, bool more) {
    if (chunk_type == P1_TX_INIT) {
        explicit_bzero(&G_context, sizeof(G_context));
        G_context.req_type = REQUEST_SIGN_TRANSACTION;
        G_context.state.tx_state = TX_STATE_NONE;
        return handle_tx_init_apdu(cdata);

    } else {  // parse transaction data chunks
        if (G_context.req_type != REQUEST_SIGN_TRANSACTION) {
            return send_error_and_reset(SWO_BAD_STATE);
        }

        // Handle chunk accumulation
        int result = handle_tx_data_chunk(cdata, more);
        TRACE("chunk result=0x%04x, more=%d", result, more);
        if (more || result != SWO_SUCCESS) {
            return result;
        }

        // Final chunk - parse and build hash
        LEDGER_ASSERT(G_context.state.tx_state == TX_STATE_CHUNKS, "Bad state before parse");
        G_context.state.tx_state = TX_STATE_RECEIVED;

        LEDGER_ASSERT(G_context.tx_info.raw_tx != NULL, "Raw transaction buffer missing");

        buffer_t buf = {
            .ptr = G_context.tx_info.raw_tx,
            .size = G_context.tx_info.raw_tx_len,
            .offset = 0
        };

        parser_status_e parse_status = parse_tx(&buf, &G_context.tx_info.transaction);
        if (parse_status != PARSING_OK) {
            return tx_handle_parse_error(parse_status);
        }
        G_context.state.tx_state = TX_STATE_PARSED;
        tx_ui_plan_t ui_plan = {0};
        int plan_result = compute_tx_hash_and_plan_ui(&ui_plan);
        if (plan_result != SWO_SUCCESS) {
            tx_context_cleanup();
            return plan_result;
        }

        G_context.state.tx_state = TX_STATE_HASHED;

        LEDGER_ASSERT(ui_plan.pair_count > 0, "Invalid UI plan");
        G_context.tx_info.planned_ui_pairs = ui_plan.pair_count;

        int ui_prep_result = ui_prepare_transaction_review();
        if (ui_prep_result != SWO_SUCCESS) {
            tx_review_cleanup();
            tx_context_cleanup();
            return ui_prep_result;
        }

        G_context.state.tx_state = TX_STATE_UI_PREPARED;
        int ui_result = ui_display_transaction();
        TRACE("ui_display_transaction result=0x%04x", ui_result);
        return ui_result;
    }
}

// All witnesses processed
void finalize_witness()
{
    // Witness confirmed - send signature back
    io_send_response_pointer(
        G_context.tx_info.witness_signature,
        ED25519_SIGNATURE_LENGTH,
        SWO_SUCCESS
    );
    G_context.tx_info.current_witness++;
    if (G_context.tx_info.current_witness == G_context.tx_info.num_witnesses) {
        tx_context_cleanup();
        G_context.req_type = REQUEST_NONE;
        G_context.state.tx_state = TX_STATE_NONE;
        TRACE("Calling nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, ui_menu_main)");
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, ui_menu_main);
    } else {
        TRACE("Calling nbgl_useCaseSpinner(\"Processing\")");
        nbgl_useCaseSpinner("Processing");
    }
}

int handler_sign_tx_witness(buffer_t *cdata) {
    // Verify we're in correct state for witness signing
    if (G_context.req_type != REQUEST_SIGN_TRANSACTION) {
        TRACE("Bad request type for witness signing: %d", G_context.req_type);
        return send_error_and_reset(SWO_BAD_STATE);
    }

    if (G_context.state.tx_state != TX_STATE_APPROVED) {
        TRACE("Bad state for witness signing: expected TX_STATE_APPROVED, got %d", G_context.state.tx_state);
        tx_context_cleanup();
        return send_error_and_reset(SWO_BAD_STATE);
    }

    // Check that we haven't exceeded the expected number of witnesses
    if (G_context.tx_info.current_witness >= G_context.tx_info.num_witnesses) {
        TRACE("Witness count exceeded: current=%d, expected=%d",
              G_context.tx_info.current_witness,
              G_context.tx_info.num_witnesses
        );
        tx_context_cleanup();
        return send_error_and_reset(SWO_BAD_STATE);
    }

    // Parse witness path from APDU data
    // buffer_read_bip44_path reads the length byte and all path components
    if (!buffer_read_bip44_path(cdata, &G_context.tx_info.witness_path)) {
        tx_context_cleanup();
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }

    TRACE("Witness %d: path length=%d",
           G_context.tx_info.current_witness,
           G_context.tx_info.witness_path.length);

    // Check security policy for witness signing
    // Determine if mint is present in the transaction
    bool mintPresent = (G_context.tx_info.transaction.num_mint_asset_groups > 0);

    // Get pool owner path if this is a pool registration
    const bip44_path_t* poolOwnerPath = NULL;
    if (G_context.tx_info.transaction.txSigningMode == SIGN_TX_SIGNINGMODE_POOL_REGISTRATION_OWNER ||
        G_context.tx_info.transaction.txSigningMode == SIGN_TX_SIGNINGMODE_POOL_REGISTRATION_OPERATOR) {
        // Extract pool owner path from pool registration certificate if available
        // For POOL_REGISTRATION_OWNER mode, we need to validate that the witness path matches one of the pool owners
        // The security policy will handle the validation
        for (s_flist_node* node = G_context.tx_info.transaction.certificates; node != NULL; node = node->next) {
            tx_certificate_list_item_t* cert_item = (tx_certificate_list_item_t*) node;
            if (cert_item->certificate_data.type == CERTIFICATE_STAKE_POOL_REGISTRATION) {
                // For POOL_REGISTRATION_OWNER mode, set poolOwnerPath to the witness path for validation
                // For POOL_REGISTRATION_OPERATOR mode, we don't need pool owner path
                if (G_context.tx_info.transaction.txSigningMode == SIGN_TX_SIGNINGMODE_POOL_REGISTRATION_OWNER) {
                    poolOwnerPath = &G_context.tx_info.witness_path;
                }
                break;
            }
        }
    }

    warning_bits_t witness_warnings;
    warning_bits_init(&witness_warnings);
    security_policy_t policy = policyForSignTxWitness(
        G_context.tx_info.transaction.txSigningMode,
        &G_context.tx_info.witness_path,
        mintPresent,
        poolOwnerPath,
        &witness_warnings
    );

    TRACE("Witness security policy: %d", (int) policy);

    // Handle DENY policy
    if (policy == POLICY_DENY) {
        TRACE("Security policy DENY - rejecting witness");
        tx_context_cleanup();
        return send_error_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
    }

    // Sign the transaction hash with the witness path
    getWitness(&G_context.tx_info.witness_path,
               G_context.tx_info.tx_hash,
               sizeof(G_context.tx_info.tx_hash),
               G_context.tx_info.witness_signature,
               sizeof(G_context.tx_info.witness_signature));

    TRACE("Witness signature: %.*H", ED25519_SIGNATURE_LENGTH, G_context.tx_info.witness_signature);

    if (policy == POLICY_HIDE) {
        finalize_witness();
        return 0;
    }

    if (policy == POLICY_SHOW) {
        return ui_display_witness(&G_context.tx_info.witness_path, policy, witness_warnings);
    }

    ASSERT(false);
    tx_context_cleanup();
    return send_error_and_reset(SWO_BAD_STATE);
}
