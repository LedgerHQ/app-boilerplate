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

#include "sign_tx.h"
#include "cardano_swo.h"
#include "globals.h"
#include "display.h"
#include "tx_types.h"
#include "tx_output_types.h"
#include "tx_parse.h"
#include "memory/mem.h"
#include "constants.h"
#include "types.h"
#include "utils/utils.h"
#include "utils/cardano_os_utils.h"
#include "utils/cbor.h"
#include "txHashBuilder/txHashBuilder.h"
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

    // Field 4 (certificates) - optional, not implemented yet
    uint16_t num_certificates_dummy;
    if (!buffer_read_u16(cdata, &num_certificates_dummy, BE)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }

    // Field 5 (withdrawals) - optional
    if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_withdrawals, BE)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }

    // Field 7 (auxiliary data hash) - optional, not implemented yet
    uint8_t includeAuxDataHashByte;
    bool includeAuxDataHash = false;
    if (!buffer_read_u8(cdata, &includeAuxDataHashByte)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeAuxDataHashByte, &includeAuxDataHash)) {
        return send_error_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
    }
    // TODO: Store includeAuxDataHash when auxiliary data is implemented

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

    // Field 11 (script data hash) - optional, not implemented yet
    uint8_t includeScriptDataHashByte;
    bool includeScriptDataHash = false;
    if (!buffer_read_u8(cdata, &includeScriptDataHashByte)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeScriptDataHashByte, &includeScriptDataHash)) {
        return send_error_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
    }
    // TODO: Store includeScriptDataHash when script data hash is implemented

    // Field 13 (collateral inputs) - optional, not implemented yet
    uint16_t num_collateral_inputs_dummy;
    if (!buffer_read_u16(cdata, &num_collateral_inputs_dummy, BE)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }

    // Field 14 (required signers) - optional, not implemented yet
    uint16_t num_required_signers_dummy;
    if (!buffer_read_u16(cdata, &num_required_signers_dummy, BE)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }

    // Field 15 (network ID) - optional, not implemented yet
    uint8_t includeNetworkIdByte;
    bool includeNetworkId = false;
    if (!buffer_read_u8(cdata, &includeNetworkIdByte)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeNetworkIdByte, &includeNetworkId)) {
        return send_error_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
    }
    // TODO: Store includeNetworkId when network ID is implemented

    // Field 16 (collateral output) - optional, not implemented yet
    uint8_t includeCollateralOutputByte;
    bool includeCollateralOutput = false;
    if (!buffer_read_u8(cdata, &includeCollateralOutputByte)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeCollateralOutputByte, &includeCollateralOutput)) {
        return send_error_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
    }
    // TODO: Store includeCollateralOutput when collateral output is implemented

    // Field 17 (total collateral) - optional, not implemented yet
    uint8_t includeTotalCollateralByte;
    bool includeTotalCollateral = false;
    if (!buffer_read_u8(cdata, &includeTotalCollateralByte)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeTotalCollateralByte, &includeTotalCollateral)) {
        return send_error_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
    }
    // TODO: Store includeTotalCollateral when total collateral is implemented

    // Field 18 (reference inputs) - optional, not implemented yet
    uint16_t num_reference_inputs_dummy;
    if (!buffer_read_u16(cdata, &num_reference_inputs_dummy, BE)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }

    // Field 19 (voting procedures) - optional, not implemented yet
    uint16_t num_voting_procedures_dummy;
    if (!buffer_read_u16(cdata, &num_voting_procedures_dummy, BE)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }

    // Field 21 (treasury) - optional, not implemented yet
    uint8_t includeTreasuryByte;
    bool includeTreasury = false;
    if (!buffer_read_u8(cdata, &includeTreasuryByte)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeTreasuryByte, &includeTreasury)) {
        return send_error_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
    }
    // TODO: Store includeTreasury when treasury is implemented

    // Field 22 (donation) - optional, not implemented yet
    uint8_t includeDonationByte;
    bool includeDonation = false;
    if (!buffer_read_u8(cdata, &includeDonationByte)) {
        return send_error_and_reset(SWO_WRONG_DATA_LENGTH);
    }
    if (!parseIncluded(includeDonationByte, &includeDonation)) {
        return send_error_and_reset(SWO_TX_PARSING_FAIL_INCLUSION_FLAG);
    }
    // TODO: Store includeDonation when donation is implemented

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
    security_policy_t init_policy = policyForSignTxInit(
        G_context.tx_info.transaction.txSigningMode,
        G_context.tx_info.transaction.networkId,
        G_context.tx_info.transaction.protocolMagic,
        G_context.tx_info.transaction.num_outputs,
        0,      // numCertificates - not implemented yet
        G_context.tx_info.transaction.num_withdrawals,
        false,  // includeMint - not implemented yet
        false,  // includeScriptDataHash - not implemented yet
        0,      // numCollateralInputs - not implemented yet
        0,      // numRequiredSigners - not implemented yet
        false,  // includeNetworkId - not implemented yet
        false,  // includeCollateralOutput - not implemented yet
        false,  // includeTotalCollateral - not implemented yet
        0,      // numReferenceInputs - not implemented yet
        0,      // numVotingProcedures - not implemented yet
        false,  // includeTreasury - not implemented yet
        false,  // includeDonation - not implemented yet
        &G_context.tx_info.warning_bits);

    TRACE("Transaction init security policy: %d", (int) init_policy);

    if (init_policy == POLICY_DENY) {
        TRACE("Security policy DENY - rejecting transaction init");
        return send_error_and_reset(SWO_SECURITY_CONDITION_NOT_SATISFIED);
    }

    // Show spinner to indicate transaction data is being processed
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

static int parse_transaction_buffer(void) {
    LEDGER_ASSERT(G_context.state.tx_state == TX_STATE_RECEIVED, "Parsing invoked at wrong state");
    buffer_t buf = {.ptr = G_context.tx_info.raw_tx,
                    .size = G_context.tx_info.raw_tx_len,
                    .offset = 0};

    parser_status_e status = parse_tx(&buf, &G_context.tx_info.transaction);
    TRACE("Parsing status: %d", status);
    if (status != PARSING_OK) {
        tx_context_cleanup();
        switch (status) {
            case INPUTS_PARSING_ERROR:
            case INPUTS_COUNT_PARSING_ERROR:
                return send_error_and_reset(SWO_TX_PARSING_FAIL_INPUTS);
            case OUTPUTS_PARSING_ERROR:
            case OUTPUTS_COUNT_PARSING_ERROR:
            case OUTPUT_DESTINATION_TYPE_ERROR:
            case OUTPUT_ADDRESS_SIZE_ERROR:
                return send_error_and_reset(SWO_TX_PARSING_FAIL_OUTPUTS);
            case FEE_PARSING_ERROR:
                return send_error_and_reset(SWO_TX_PARSING_FAIL_FEE);
            case TTL_PARSING_ERROR:
                return send_error_and_reset(SWO_TX_PARSING_FAIL_TTL);
            case VALIDITY_INTERVAL_START_PARSING_ERROR:
                return send_error_and_reset(SWO_TX_PARSING_FAIL_VALIDITY_INTERVAL_START);
            case TX_SIZE_TOO_LARGE_ERROR:
                return send_error_and_reset(SWO_INVALID_TX_LENGTH);
            case TX_BUFFER_NOT_FULLY_CONSUMED_ERROR:
                return send_error_and_reset(SWO_TX_PARSING_FAIL_BUFFER_NOT_FULLY_CONSUMED);
            default:
                return send_error_and_reset(SWO_TX_PARSING_FAIL);
        }
    }

    // Fill in Byron protocol magic for DEVICE_OWNED outputs
    s_flist_node *output_node = G_context.tx_info.transaction.outputs;
    while (output_node != NULL) {
        tx_output_list_item_t *output_item = (tx_output_list_item_t *) output_node;
        if (output_item->output_data.destination.type == DESTINATION_DEVICE_OWNED &&
            output_item->output_data.destination.params.type == BYRON) {
            output_item->output_data.destination.params.protocolMagic =
                G_context.tx_info.transaction.protocolMagic;
        }
        output_node = output_node->next;
    }

    // Check for high fee warning
    if (G_context.tx_info.transaction.fee > HIGH_FEE_WARNING_THRESHOLD) {
        TRACE("High fee detected: %llu lovelace (threshold: %u lovelace)",
              G_context.tx_info.transaction.fee, HIGH_FEE_WARNING_THRESHOLD);
        warning_bits_set(&G_context.tx_info.warning_bits, WARNING_BIT_HIGH_FEE);
    }

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
        if (more || result != SWO_SUCCESS) {
            return result;
        }

        // Final chunk - parse and build hash
        LEDGER_ASSERT(G_context.state.tx_state == TX_STATE_CHUNKS, "Bad state before parse");
        G_context.state.tx_state = TX_STATE_RECEIVED;
        int parse_result = parse_transaction_buffer();
        if (parse_result != SWO_SUCCESS) {
            return parse_result;
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
        return ui_display_transaction();
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
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, ui_menu_main);
    } else {
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
    bool mintPresent = false; // TODO mint not implemented yet

    // Get pool owner path if this is a pool registration
    const bip44_path_t* poolOwnerPath = NULL;
    if (G_context.tx_info.transaction.txSigningMode == SIGN_TX_SIGNINGMODE_POOL_REGISTRATION_OWNER ||
        G_context.tx_info.transaction.txSigningMode == SIGN_TX_SIGNINGMODE_POOL_REGISTRATION_OPERATOR) {
        // Extract pool owner path from pool registration certificate if available
        // TODO For now, we'll pass NULL and let the policy handle it
        poolOwnerPath = NULL;
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
