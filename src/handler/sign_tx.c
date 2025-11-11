/*****************************************************************************
 *   Ledger App Boilerplate.
 *   (c) 2020 Ledger SAS.
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
#include "cx.h"
#include "buffer.h"

#include "sign_tx.h"
#include "sw.h"
#include "globals.h"
#include "display.h"
#include "tx_types.h"
#include "tx_output_types.h"
#include "tx_warnings.h"
#include "deserialize.h"
#include "mem.h"
#include "constants.h"
#include "utils/utils.h"
#include "txHashBuilder/txHashBuilder.h"
#include "cardano.h"
#include "messageSigning.h"
#include "securityPolicy.h"
#include "dispatcher.h"

int handler_sign_tx(buffer_t *cdata, uint8_t chunk_type, bool more) {
    // Special chunk type for INIT APDU (contains description, not tx data)
    if (chunk_type == P1_TX_INIT) {  // INIT APDU: parse transaction description
        explicit_bzero(&G_context, sizeof(G_context));
        G_context.req_type = REQUEST_CONFIRM_TRANSACTION;
        G_context.state = STATE_NONE;
        G_context.tx_info.raw_tx = NULL;  // Initialize pointer to NULL
        G_context.tx_info.raw_tx_len = 0;
        G_context.tx_info.warning_list = NULL;  // Initialize warning list

        // Read transaction options (8 bytes)
        uint64_t options;
        if (!buffer_read_u64(cdata, &options, BE)) {
            return io_send_sw(SW_WRONG_DATA_LENGTH);
        }

        // Parse options - extract tagCborSets flag
        bool tagCborSets = options & TX_OPTIONS_TAG_CBOR_SETS;
        options &= ~TX_OPTIONS_TAG_CBOR_SETS;
        // Validate no unknown flags are set
        if (options != 0) {
            return io_send_sw(SW_WRONG_DATA_LENGTH);  // Unknown options
        }
        G_context.tx_info.transaction.tagCborSets = tagCborSets;

        // Read transaction metadata
        uint8_t txSigningMode;
        if (!buffer_read_u8(cdata, &txSigningMode)) {
            return io_send_sw(SW_WRONG_DATA_LENGTH);
        }
        G_context.tx_info.transaction.txSigningMode = (sign_tx_signingmode_t) txSigningMode;

        // Read network parameters (networkId: 1 byte, protocolMagic: 4 bytes)
        if (!buffer_read_u8(cdata, &G_context.tx_info.transaction.networkId) ||
            !buffer_read_u32(cdata, &G_context.tx_info.transaction.protocolMagic, BE)) {
            return io_send_sw(SW_WRONG_DATA_LENGTH);
        }

        // Read transaction structure counts
        if (!buffer_read_u16(cdata, &G_context.tx_info.transaction.num_inputs, BE) ||
            !buffer_read_u16(cdata, &G_context.tx_info.transaction.num_outputs, BE)) {
            return io_send_sw(SW_WRONG_DATA_LENGTH);
        }

        // Read optional field flags
        uint8_t includeTtlByte;
        if (!buffer_read_u8(cdata, &includeTtlByte)) {
            return io_send_sw(SW_WRONG_DATA_LENGTH);
        }
        if (!parseIncluded(includeTtlByte, &G_context.tx_info.transaction.includeTtl)) {
            return io_send_sw(SW_TX_PARSING_FAIL_INCLUSION_FLAG);  // Invalid inclusion flag value
        }

        PRINTF("TX Mode=%d, Network: ID=%d, Magic=%d, Inputs=%d, Outputs=%d, TTL=%d\n",
               G_context.tx_info.transaction.txSigningMode,
               G_context.tx_info.transaction.networkId,
               G_context.tx_info.transaction.protocolMagic,
               G_context.tx_info.transaction.num_inputs,
               G_context.tx_info.transaction.num_outputs,
               G_context.tx_info.transaction.includeTtl);

        return io_send_sw(SW_OK);

    } else {  // parse transaction data chunks (chunk >= 0)

        if (G_context.req_type != REQUEST_CONFIRM_TRANSACTION) {
            return io_send_sw(SW_BAD_STATE);
        }

        // Allocate buffer on first data chunk
        if (G_context.tx_info.raw_tx == NULL) {
            TRACE("Allocating transaction buffer: %d bytes", TX_BUFFER_SIZE);
            G_context.tx_info.raw_tx = (uint8_t *) app_mem_alloc(TX_BUFFER_SIZE);
            if (G_context.tx_info.raw_tx == NULL) {
                TRACE("Failed to allocate %d byte transaction buffer!", TX_BUFFER_SIZE);
                return io_send_sw(SW_TX_PARSING_FAIL);
            }
            TRACE("Transaction buffer allocated: %d bytes at %p", TX_BUFFER_SIZE, G_context.tx_info.raw_tx);
        }

        // Check if adding this chunk would exceed allocated buffer
        if (G_context.tx_info.raw_tx_len + cdata->size > TX_BUFFER_SIZE) {
            TRACE("Transaction too large: current=%d, chunk=%d, max=%d",
                  G_context.tx_info.raw_tx_len, cdata->size, TX_BUFFER_SIZE);
            return io_send_sw(SW_WRONG_TX_LENGTH);
        }

        if (!buffer_move(cdata,
                         G_context.tx_info.raw_tx + G_context.tx_info.raw_tx_len,
                         cdata->size)) {
            TRACE("Failed to copy transaction chunk");
            return io_send_sw(SW_TX_PARSING_FAIL);
        }
        G_context.tx_info.raw_tx_len += cdata->size;
        TRACE("Copied %d bytes, total: %d", cdata->size, G_context.tx_info.raw_tx_len);

        if (more) {
            // more APDUs with transaction part are expected.
            // Send a SW_OK to signal that we have received the chunk
            return io_send_sw(SW_OK);

        } else {
            // last APDU for this transaction, let's parse, display and request a sign confirmation

            buffer_t buf = {.ptr = G_context.tx_info.raw_tx,
                            .size = G_context.tx_info.raw_tx_len,
                            .offset = 0};

            parser_status_e status = transaction_deserialize(&buf, &G_context.tx_info.transaction);
            PRINTF("Parsing status: %d.\n", status);
            if (status != PARSING_OK) {
                // Map parser status to specific error codes
                switch (status) {
                    case INPUTS_PARSING_ERROR:
                    case INPUTS_COUNT_PARSING_ERROR:
                        return io_send_sw(SW_TX_PARSING_FAIL_INPUTS);
                    case OUTPUTS_PARSING_ERROR:
                    case OUTPUTS_COUNT_PARSING_ERROR:
                    case OUTPUT_DESTINATION_TYPE_ERROR:
                    case OUTPUT_ADDRESS_SIZE_ERROR:
                        return io_send_sw(SW_TX_PARSING_FAIL_OUTPUTS);
                    case FEE_PARSING_ERROR:
                        return io_send_sw(SW_TX_PARSING_FAIL_FEE);
                    case TO_PARSING_ERROR:
                        return io_send_sw(SW_TX_PARSING_FAIL_TTL);
                    default:
                        return io_send_sw(SW_TX_PARSING_FAIL);
                }
            }

            G_context.state = STATE_PARSED;

            // Fill in network params for DEVICE_OWNED outputs
            s_flist_node *output_node = G_context.tx_info.transaction.outputs;
            while (output_node != NULL) {
                tx_output_list_item_t *output_item = (tx_output_list_item_t *) output_node;
                if (output_item->output_data.destination.type == DESTINATION_DEVICE_OWNED) {
                    output_item->output_data.destination.params.networkId =
                        G_context.tx_info.transaction.networkId;
                    output_item->output_data.destination.params.protocolMagic =
                        G_context.tx_info.transaction.protocolMagic;
                }
                output_node = output_node->next;
            }

            // Collect warnings based on network parameters
            // For now, skip warning collection - will be implemented when needed
            // Network validation functions are in securityPolicy.c which has complex dependencies

            // Build transaction hash using txHashBuilder (local variable to avoid includes in types.h)
            tx_hash_builder_t txHashBuilder;
            explicit_bzero(&txHashBuilder, sizeof(txHashBuilder));

            // Initialize txHashBuilder with inputs, outputs, fee, and optionally TTL
            txHashBuilder_init(&txHashBuilder,
                              G_context.tx_info.transaction.tagCborSets,  // tagCborSets
                              G_context.tx_info.transaction.num_inputs,   // numInputs
                              G_context.tx_info.transaction.num_outputs,  // numOutputs
                              G_context.tx_info.transaction.includeTtl,   // includeTtl
                              0,      // numCertificates
                              0,      // numWithdrawals
                              false,  // includeAuxData
                              false,  // includeValidityIntervalStart
                              false,  // includeMint
                              false,  // includeScriptDataHash
                              0,      // numCollateralInputs
                              0,      // numRequiredSigners
                              false,  // includeNetworkId
                              false,  // includeCollateralOutput
                              false,  // includeTotalCollateral
                              0,      // numReferenceInputs
                              0,      // numVotingProcedures
                              false,  // includeTreasury
                              false); // includeDonation

            // Add inputs to hash builder
            txHashBuilder_enterInputs(&txHashBuilder);
            s_flist_node *node = G_context.tx_info.transaction.inputs;
            while (node != NULL) {
                tx_input_list_item_t *item = (tx_input_list_item_t *) node;
                // Cast the embedded struct to tx_input_t* - memory layout is identical
                txHashBuilder_addInput(&txHashBuilder, (const tx_input_t*)&item->input_data);
                node = node->next;
            }

            // Add outputs to hash builder
            txHashBuilder_enterOutputs(&txHashBuilder);
            node = G_context.tx_info.transaction.outputs;
            while (node != NULL) {
                tx_output_list_item_t *output_item = (tx_output_list_item_t *) node;

                // Prepare output description for hash builder
                tx_output_description_t output_desc;
                output_desc.format = ARRAY_LEGACY;  // Simple format for now
                output_desc.destination.type = output_item->output_data.destination.type;
                output_desc.destination.address.buffer = output_item->output_data.destination.address.buffer;
                output_desc.destination.address.size = output_item->output_data.destination.address.size;
                output_desc.amount = output_item->output_data.adaAmount;
                output_desc.numAssetGroups = 0;  // No tokens yet
                output_desc.includeDatum = false;
                output_desc.includeRefScript = false;

                txHashBuilder_addOutput_topLevelData(&txHashBuilder, &output_desc);

                node = node->next;
            }

            // Add fee to hash builder
            txHashBuilder_addFee(&txHashBuilder, G_context.tx_info.transaction.fee);

            // Add TTL to hash builder if included
            if (G_context.tx_info.transaction.includeTtl) {
                txHashBuilder_addTtl(&txHashBuilder, G_context.tx_info.transaction.ttl);
            }

            // Finalize the hash
            txHashBuilder_finalize(&txHashBuilder,
                                  G_context.tx_info.tx_hash,
                                  sizeof(G_context.tx_info.tx_hash));

            PRINTF("Hash: %.*H\n", sizeof(G_context.tx_info.tx_hash), G_context.tx_info.tx_hash);

            return ui_display_transaction();
        }
    }
    return 0;
}

int handler_sign_tx_witness(buffer_t *cdata) {
    // Verify we're in correct state for witness signing
    if (G_context.state != STATE_APPROVED || G_context.req_type != REQUEST_CONFIRM_TRANSACTION) {
        return io_send_sw(SW_BAD_STATE);
    }

    // Check that we haven't exceeded the expected number of witnesses
    if (G_context.tx_info.current_witness >= G_context.tx_info.num_witnesses) {
        TRACE("Witness count exceeded: current=%d, expected=%d",
              G_context.tx_info.current_witness,
              G_context.tx_info.num_witnesses);
        return io_send_sw(SW_WRONG_DATA_LENGTH);
    }

    // Parse witness path from APDU data
    uint8_t path_len;
    if (!buffer_read_u8(cdata, &path_len) ||
        !buffer_read_bip32_path(cdata,
                                G_context.tx_info.witness_path.path,
                                (size_t) path_len)) {
        return io_send_sw(SW_WRONG_DATA_LENGTH);
    }
    G_context.tx_info.witness_path.length = path_len;

    PRINTF("Witness %d: path length=%d\n",
           G_context.tx_info.current_witness,
           path_len);

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

    security_policy_t policy = policyForSignTxWitness(
        G_context.tx_info.transaction.txSigningMode,
        &G_context.tx_info.witness_path,
        mintPresent,
        poolOwnerPath
    );

    TRACE("Witness security policy: %d", (int) policy);

    // Handle DENY policy
    if (policy == POLICY_DENY) {
        TRACE("Security policy DENY - rejecting witness");
        return io_send_sw(ERR_REJECTED_BY_POLICY);
    }

    // Sign the transaction hash with the witness path
    getWitness(&G_context.tx_info.witness_path,
               G_context.tx_info.tx_hash,
               sizeof(G_context.tx_info.tx_hash),
               G_context.tx_info.witness_signature,
               sizeof(G_context.tx_info.witness_signature));

    PRINTF("Witness signature: %.*H\n", ED25519_SIGNATURE_LENGTH, G_context.tx_info.witness_signature);

    // For SHOW and PROMPT policies, display witness to user before returning signature
    if (policy == POLICY_SHOW_BEFORE_RESPONSE || policy == POLICY_PROMPT_BEFORE_RESPONSE ||
        policy == POLICY_PROMPT_WARN_UNUSUAL) {
        // Display witness path and request user confirmation
        return ui_display_witness(&G_context.tx_info.witness_path, policy);
    }

    // For ALLOW_WITHOUT_PROMPT, increment counter and send signature directly
    G_context.tx_info.current_witness++;

    // Send signature back to client
    return io_send_response_pointer(G_context.tx_info.witness_signature,
                                    ED25519_SIGNATURE_LENGTH,
                                    SW_OK);
}
