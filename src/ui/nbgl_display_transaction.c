/*****************************************************************************
 *   Ledger Cardano App
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

#include <stdbool.h>  // bool
#include <string.h>   // strlen, memmove

#include "os.h"
#include "glyphs.h"
#include "nbgl_use_case.h"
#include "io.h"

#include "display.h"
#include "constants.h"
#include "globals.h"
#include "utils/cardano_os_utils.h"
#include "cardano_swo.h"
#include "tx_types.h"
#include "tx_output_types.h"
#include "tx_warnings.h"
#include "menu.h"
#include "addressUtils/addressUtilsShelley.h"
#include "nbgl_screens.h"
#include "transaction/tx_parse.h"
#include "utils/textUtils.h"
#include "ui_utils.h"

// Dynamically allocated buffers for transaction display (declared in ui_display_transaction)
static nbgl_contentCenter_t *g_warningInfo = NULL;
static nbgl_warningDetails_t *g_warningDetails = NULL;
static nbgl_warning_t *g_warning = NULL;

/**
 * Cleanup dynamically allocated buffers for transaction display
 */
void tx_review_cleanup(void) {
    ui_cleanup_tracked_allocations();
    ui_pairs_cleanup();
    tx_warning_list_cleanup((tx_warning_list_item_t **)&G_context.tx_info.warning_list);
}

// called when long press button on 3rd page is long-touched or when reject footer is touched
static void tx_review_choice(bool confirm) {
    if (confirm) {
        // User approved transaction
        // Set state to APPROVED and return tx hash
        // Witnesses will be handled in separate APDUs
        G_context.state.tx_state = TX_STATE_APPROVED;

        // Initialize witness counters
        G_context.tx_info.current_witness = 0;
        // num_witnesses is set during P1_TX_INIT (sign_tx.c:118)

        // Send tx hash back to client
        io_send_response_pointer(G_context.tx_info.tx_hash, sizeof(G_context.tx_info.tx_hash), SWO_SUCCESS);

        // Check if there are witnesses to process
        if (G_context.tx_info.num_witnesses > 0) {
            // Witnesses coming - clean up NBGL display and warnings but keep transaction context
            // (tx hash and parsed tx needed for witness signing)
            tx_review_cleanup();
            // Show spinner while waiting for witness APDUs
            nbgl_useCaseSpinner("Processing");
        } else {
            tx_review_cleanup();
            tx_context_cleanup();
            G_context.state.tx_state = TX_STATE_NONE;
            G_context.req_type = REQUEST_NONE;  // Reset to idle
            nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_SIGNED, ui_menu_main);
        }
    } else {
        // User rejected
        tx_review_cleanup();
        tx_context_cleanup();
        G_context.state.tx_state = TX_STATE_NONE;
        G_context.req_type = REQUEST_NONE;

        io_send_sw(SWO_CONDITIONS_NOT_SATISFIED);
        nbgl_useCaseReviewStatus(STATUS_TYPE_TRANSACTION_REJECTED, ui_menu_main);
    }
}

// Public function to start the transaction review
// - Check if the app is in the right state for transaction review
// - Format the fee and output strings dynamically
// - Display the first screen of the transaction review
int ui_display_transaction(void) {
    if (G_context.req_type != REQUEST_SIGN_TRANSACTION || G_context.state.tx_state != TX_STATE_PARSED) {
        G_context.state.tx_state = TX_STATE_NONE;
        return send_error_and_reset(SWO_BAD_STATE);
    }

    // Allocate display buffers using ui_mem_alloc for automatic tracking
    char *fee = (char *) ui_mem_alloc(MAX_ADA_AMOUNT_STRING_SIZE);
    if (fee == NULL) {
        tx_review_cleanup();
        return send_error_and_reset(SWO_INSUFFICIENT_MEMORY);
    }
    char *ttl = (char *) ui_mem_alloc(MAX_ADA_AMOUNT_STRING_SIZE);
    if (ttl == NULL) {
        tx_review_cleanup();
        return send_error_and_reset(SWO_INSUFFICIENT_MEMORY);
    }
    char *warning_msg = (char *) ui_mem_alloc(MAX_WARNING_MESSAGE_SIZE);
    if (warning_msg == NULL) {
        tx_review_cleanup();
        return send_error_and_reset(SWO_INSUFFICIENT_MEMORY);
    }
    // Initialize to empty string (null-terminated) in case no warnings are present
    explicit_bzero(warning_msg, MAX_WARNING_MESSAGE_SIZE);
    warning_msg[0] = '\0';

    // Calculate number of pairs dynamically based on security policies
    // Start with: fee + (TTL if included) + tx hash
    uint16_t num_pairs = 2;
    if (G_context.tx_info.transaction.includeTtl) {
        num_pairs++;  // Add 1 for TTL
    }

    // Count withdrawals that will be displayed based on security policy
    s_flist_node *temp_node = G_context.tx_info.transaction.withdrawals;
    while (temp_node != NULL) {
        tx_withdrawal_list_item_t *temp_item = (tx_withdrawal_list_item_t *) temp_node;
        security_policy_t policy = policyForSignTxWithdrawal(
            G_context.tx_info.transaction.txSigningMode,
            &temp_item->withdrawal_data.stakeCredential
        );
        if (policy != POLICY_DENY) {
            // Withdrawal will be displayed: number + amount + reward account
            num_pairs += 3;
        }
        temp_node = temp_node->next;
    }

    // Count outputs that will be displayed based on security policy
    temp_node = G_context.tx_info.transaction.outputs;
    while (temp_node != NULL) {
        tx_output_list_item_t *temp_output = (tx_output_list_item_t *) temp_node;

        // Build output description for policy checking
        tx_output_description_t output_desc;
        output_desc.format = temp_output->output_data.format;
        output_desc.amount = temp_output->output_data.adaAmount;
        output_desc.numAssetGroups = temp_output->output_data.numAssetGroups;
        output_desc.includeDatum = (temp_output->output_data.datum.type != 0xFF);
        output_desc.includeRefScript = temp_output->output_data.hasRefScript;

        if (temp_output->output_data.destination.type == DESTINATION_THIRD_PARTY) {
            output_desc.destination.type = DESTINATION_THIRD_PARTY;
            output_desc.destination.address.buffer = temp_output->output_data.destination.address.buffer;
            output_desc.destination.address.size = temp_output->output_data.destination.address.size;
        } else if (temp_output->output_data.destination.type == DESTINATION_DEVICE_OWNED) {
            output_desc.destination.type = DESTINATION_DEVICE_OWNED;
            output_desc.destination.params = &temp_output->output_data.destination.params;
        }

        // Check output policy
        security_policy_t output_policy;
        if (temp_output->output_data.destination.type == DESTINATION_THIRD_PARTY) {
            output_policy = policyForSignTxOutputAddressBytes(
                &output_desc,
                G_context.tx_info.transaction.txSigningMode,
                G_context.tx_info.transaction.networkId,
                G_context.tx_info.transaction.protocolMagic
            );
        } else {
            output_policy = policyForSignTxOutputAddressParams(
                &output_desc,
                G_context.tx_info.transaction.txSigningMode,
                G_context.tx_info.transaction.networkId,
                G_context.tx_info.transaction.protocolMagic
            );
        }

        if (output_policy != POLICY_DENY) {
            // Output will be displayed: number + address + amount
            num_pairs += 3;
        }
        temp_node = temp_node->next;
    }

    // Initialize common pairs structure
    if (!ui_pairs_init(num_pairs)) {
        tx_review_cleanup();
        return send_error_and_reset(SWO_TX_PARSING_FAIL);
    }

    uint16_t pair_idx = 0;

    // Add fee first
    str_formatAdaAmount(G_context.tx_info.transaction.fee, fee, MAX_ADA_AMOUNT_STRING_SIZE);
    g_pairs[pair_idx].item = "Fee";
    g_pairs[pair_idx].value = fee;
    pair_idx++;

    // Add TTL if included
    if (G_context.tx_info.transaction.includeTtl) {
        ui_getUint64Screen(ttl, MAX_ADA_AMOUNT_STRING_SIZE, G_context.tx_info.transaction.ttl);
        g_pairs[pair_idx].item = "TTL";
        g_pairs[pair_idx].value = ttl;
        pair_idx++;
    }

    // Add each withdrawal (1-indexed for display)
    uint16_t withdrawal_num = 1;
    s_flist_node *withdrawal_node = G_context.tx_info.transaction.withdrawals;
    while (withdrawal_node != NULL) {
        tx_withdrawal_list_item_t *withdrawal_item = (tx_withdrawal_list_item_t *) withdrawal_node;

        // Check security policy for this withdrawal
        security_policy_t policy = policyForSignTxWithdrawal(
            G_context.tx_info.transaction.txSigningMode,
            &withdrawal_item->withdrawal_data.stakeCredential
        );

        // Only display withdrawal if policy allows it
        if (policy != POLICY_DENY) {
            // Add withdrawal number pair (e.g., "Withdrawal" | "#1")
            char *withdrawal_num_str = (char *) ui_mem_alloc(MAX_UINT64_STRING_SIZE);
            if (withdrawal_num_str == NULL) {
                tx_review_cleanup();
                return send_error_and_reset(SWO_INSUFFICIENT_MEMORY);
            }
            snprintf(withdrawal_num_str, MAX_UINT64_STRING_SIZE, "#%d", withdrawal_num);
            g_pairs[pair_idx].item = "Withdrawal";
            g_pairs[pair_idx].value = withdrawal_num_str;
            pair_idx++;

            // Add amount pair with label "Amount"
            g_pairs[pair_idx].item = "Amount";

            char *withdrawal_amount_str = (char *) ui_mem_alloc(MAX_ADA_AMOUNT_STRING_SIZE);
            if (withdrawal_amount_str == NULL) {
                tx_review_cleanup();
                return send_error_and_reset(SWO_INSUFFICIENT_MEMORY);
            }
            if (!str_formatAdaAmount(withdrawal_item->withdrawal_data.amount, withdrawal_amount_str, MAX_ADA_AMOUNT_STRING_SIZE)) {
                return send_error_and_reset(SWO_DISPLAY_AMOUNT_FAIL);
            }
            g_pairs[pair_idx].value = withdrawal_amount_str;
            pair_idx++;

            // Add reward account pair
            g_pairs[pair_idx].item = "Reward account";

            char *reward_account_str = (char *) ui_mem_alloc(MAX_HUMAN_ADDRESS_SIZE);
            if (reward_account_str == NULL) {
                tx_review_cleanup();
                return send_error_and_reset(SWO_INSUFFICIENT_MEMORY);
            }

            size_t reward_addr_len = 0;
            uint8_t reward_addr_bytes[REWARD_ACCOUNT_SIZE];

            switch (withdrawal_item->withdrawal_data.stakeCredential.type) {
                case EXT_CREDENTIAL_KEY_PATH: {
                    // Construct reward address from path
                    reward_addr_len = constructRewardAddressFromKeyPath(
                        &withdrawal_item->withdrawal_data.stakeCredential.keyPath,
                        G_context.tx_info.transaction.networkId,
                        reward_addr_bytes,
                        sizeof(reward_addr_bytes)
                    );
                    break;
                }
                case EXT_CREDENTIAL_KEY_HASH: {
                    // Construct reward address from key hash
                    reward_addr_len = constructRewardAddressFromHash(
                        G_context.tx_info.transaction.networkId,
                        REWARD_HASH_SOURCE_KEY,
                        withdrawal_item->withdrawal_data.stakeCredential.keyHash,
                        ADDRESS_KEY_HASH_LENGTH,
                        reward_addr_bytes,
                        sizeof(reward_addr_bytes)
                    );
                    break;
                }
                case EXT_CREDENTIAL_SCRIPT_HASH: {
                    // Construct reward address from script hash
                    reward_addr_len = constructRewardAddressFromHash(
                        G_context.tx_info.transaction.networkId,
                        REWARD_HASH_SOURCE_SCRIPT,
                        withdrawal_item->withdrawal_data.stakeCredential.scriptHash,
                        SCRIPT_HASH_LENGTH,
                        reward_addr_bytes,
                        sizeof(reward_addr_bytes)
                    );
                    break;
                }
                default:
                    return send_error_and_reset(SWO_TX_PARSING_FAIL);
            }

            if (reward_addr_len == 0) {
                return send_error_and_reset(SWO_DISPLAY_ADDRESS_FAIL);
            }

            // Convert reward address bytes to human-readable bech32
            reward_addr_len = humanReadableAddress(
                reward_addr_bytes,
                reward_addr_len,
                reward_account_str,
                MAX_HUMAN_ADDRESS_SIZE
            );

            if (reward_addr_len == 0) {
                return send_error_and_reset(SWO_DISPLAY_ADDRESS_FAIL);
            }

            g_pairs[pair_idx].value = reward_account_str;
            pair_idx++;

            withdrawal_num++;
        }

        withdrawal_node = withdrawal_node->next;
    }

    // Add each output (1-indexed for display)
    uint16_t output_num = 1;
    s_flist_node *node = G_context.tx_info.transaction.outputs;
    while (node != NULL) {
        tx_output_list_item_t *output_item = (tx_output_list_item_t *) node;

        // Build output description for policy checking
        tx_output_description_t output_desc;
        output_desc.format = output_item->output_data.format;
        output_desc.amount = output_item->output_data.adaAmount;
        output_desc.numAssetGroups = output_item->output_data.numAssetGroups;
        output_desc.includeDatum = (output_item->output_data.datum.type != 0xFF);
        output_desc.includeRefScript = output_item->output_data.hasRefScript;

        if (output_item->output_data.destination.type == DESTINATION_THIRD_PARTY) {
            output_desc.destination.type = DESTINATION_THIRD_PARTY;
            output_desc.destination.address.buffer = output_item->output_data.destination.address.buffer;
            output_desc.destination.address.size = output_item->output_data.destination.address.size;
        } else if (output_item->output_data.destination.type == DESTINATION_DEVICE_OWNED) {
            output_desc.destination.type = DESTINATION_DEVICE_OWNED;
            output_desc.destination.params = &output_item->output_data.destination.params;
        }

        // Check output policy
        security_policy_t output_policy;
        if (output_item->output_data.destination.type == DESTINATION_THIRD_PARTY) {
            output_policy = policyForSignTxOutputAddressBytes(
                &output_desc,
                G_context.tx_info.transaction.txSigningMode,
                G_context.tx_info.transaction.networkId,
                G_context.tx_info.transaction.protocolMagic
            );
        } else {
            output_policy = policyForSignTxOutputAddressParams(
                &output_desc,
                G_context.tx_info.transaction.txSigningMode,
                G_context.tx_info.transaction.networkId,
                G_context.tx_info.transaction.protocolMagic
            );
        }

        // Only display output if policy allows it
        if (output_policy != POLICY_DENY) {
            // Add output number pair (e.g., "Output" | "#1")
            char *output_num_str = (char *) ui_mem_alloc(MAX_UINT64_STRING_SIZE);
            if (output_num_str == NULL) {
                tx_review_cleanup();
                return send_error_and_reset(SWO_INSUFFICIENT_MEMORY);
            }
            snprintf(output_num_str, MAX_UINT64_STRING_SIZE, "#%d", output_num);
            g_pairs[pair_idx].item = "Output";
            g_pairs[pair_idx].value = output_num_str;
            pair_idx++;

            // Add address pair (e.g., "Address" | "<address>")
            g_pairs[pair_idx].item = "Address";

            // Allocate and format address using Bech32 (Cardano format)
            // MAX_HUMAN_ADDRESS_SIZE is defined in cardano.h as 150
            char *addr_str = (char *) ui_mem_alloc(MAX_HUMAN_ADDRESS_SIZE);
            if (addr_str == NULL) {
                tx_review_cleanup();
                return send_error_and_reset(SWO_INSUFFICIENT_MEMORY);
            }

            size_t addr_len = 0;
            if (output_item->output_data.destination.type == DESTINATION_THIRD_PARTY) {
                // Use Cardano's humanReadableAddress function for proper Bech32 formatting
                addr_len = humanReadableAddress(
                    output_item->output_data.destination.address.buffer,
                    output_item->output_data.destination.address.size,
                    addr_str,
                    MAX_HUMAN_ADDRESS_SIZE
                );
            } else if (output_item->output_data.destination.type == DESTINATION_DEVICE_OWNED) {
                // Derive address from path
                uint8_t address_bytes[MAX_ADDRESS_SIZE];
                size_t address_size = deriveAddress(
                    &output_item->output_data.destination.params,
                    address_bytes,
                    sizeof(address_bytes)
                );

                if (address_size > 0) {
                    addr_len = humanReadableAddress(
                        address_bytes,
                        address_size,
                        addr_str,
                        MAX_HUMAN_ADDRESS_SIZE
                    );
                }
            }

            if (addr_len == 0) {
                return send_error_and_reset(SWO_DISPLAY_ADDRESS_FAIL);
            }

            g_pairs[pair_idx].value = addr_str;
            pair_idx++;

            // Add amount pair with label "Amount"
            g_pairs[pair_idx].item = "Amount";

            // Allocate and format amount with currency
            char *amount_str = (char *) ui_mem_alloc(MAX_AMOUNT_DISPLAY_SIZE);
            if (amount_str == NULL) {
                tx_review_cleanup();
                return send_error_and_reset(SWO_INSUFFICIENT_MEMORY);
            }
            if (!str_formatAdaAmount(output_item->output_data.adaAmount, amount_str, MAX_AMOUNT_DISPLAY_SIZE)) {
                return send_error_and_reset(SWO_DISPLAY_AMOUNT_FAIL);
            }
            g_pairs[pair_idx].value = amount_str;
            pair_idx++;

            output_num++;
        }

        node = node->next;
    }

    // Add transaction hash as the last item
    g_pairs[pair_idx].item = "Transaction hash";
    char *tx_hash_str = (char *) ui_mem_alloc(MAX_TX_HASH_DISPLAY_SIZE);
    if (tx_hash_str == NULL) {
        tx_review_cleanup();
        return send_error_and_reset(SWO_INSUFFICIENT_MEMORY);
    }
    ui_getHexBufferScreen(tx_hash_str, MAX_TX_HASH_DISPLAY_SIZE, G_context.tx_info.tx_hash, sizeof(G_context.tx_info.tx_hash));
    g_pairs[pair_idx].value = tx_hash_str;
    pair_idx++;

    // Check if we have warnings to display
    const nbgl_warning_t* warningPtr = NULL;
    if (!tx_warning_list_empty((tx_warning_list_item_t *)G_context.tx_info.warning_list)) {
        TRACE("Warnings detected, preparing warning display");

        // Concatenate all warning messages
        size_t offset = 0;
        tx_warning_list_item_t *warning_node = (tx_warning_list_item_t *)G_context.tx_info.warning_list;
        while (warning_node != NULL && offset < MAX_WARNING_MESSAGE_SIZE - 2) {
            const char *msg = tx_warning_get_message(warning_node->type);
            size_t msg_len = strlen(msg);
            if (offset + msg_len + 2 < MAX_WARNING_MESSAGE_SIZE) {
                if (offset > 0) {
                    warning_msg[offset++] = '\n';
                }
                memmove(warning_msg + offset, msg, msg_len);
                offset += msg_len;
            }
            warning_node = (tx_warning_list_item_t *)warning_node->node.next;
        }

        // Allocate and setup warning structures using ui_mem_alloc for automatic tracking
        g_warningInfo = (nbgl_contentCenter_t *) ui_mem_alloc(sizeof(nbgl_contentCenter_t));
        if (g_warningInfo == NULL) {
            tx_review_cleanup();
            return send_error_and_reset(SWO_INSUFFICIENT_MEMORY);
        }
        g_warningDetails = (nbgl_warningDetails_t *) ui_mem_alloc(sizeof(nbgl_warningDetails_t));
        if (g_warningDetails == NULL) {
            tx_review_cleanup();
            return send_error_and_reset(SWO_INSUFFICIENT_MEMORY);
        }
        g_warning = (nbgl_warning_t *) ui_mem_alloc(sizeof(nbgl_warning_t));
        if (g_warning == NULL) {
            tx_review_cleanup();
            return send_error_and_reset(SWO_INSUFFICIENT_MEMORY);
        }

        // Setup warning content
        g_warningInfo->icon = &WARNING_ICON;
        g_warningInfo->title = "Transaction Warning";
        g_warningInfo->description = warning_msg;

        g_warningDetails->title = "Transaction Warning";
        g_warningDetails->type = CENTERED_INFO_WARNING;
        g_warningDetails->centeredInfo.icon = &WARNING_ICON;
        g_warningDetails->centeredInfo.title = "Transaction Warning";
        g_warningDetails->centeredInfo.description = warning_msg;

        g_warning->introDetails = g_warningDetails;
        g_warning->reviewDetails = g_warningDetails;
        g_warning->info = g_warningInfo;
        g_warning->introTopRightIcon = &WARNING_ICON;
        g_warning->reviewTopRightIcon = &WARNING_ICON;
        warningPtr = g_warning;

        TRACE("Warning display prepared");
    }

    // Start review flow (with or without warnings)
    if (warningPtr != NULL) {
        // Use advanced review with warnings
        nbgl_useCaseAdvancedReview(TYPE_TRANSACTION,
                                  g_pairsList,
                                  &ICON_APP_CARDANO,
                                  "Review transaction",
                                  NULL,
                                  "Sign transaction",
                                  NULL,
                                  warningPtr,
                                  tx_review_choice);
    } else {
        // Use simple review without warnings
        nbgl_useCaseReview(TYPE_TRANSACTION,
                          g_pairsList,
                          &ICON_APP_CARDANO,
                          "Review transaction",
                          NULL,
#ifdef SCREEN_SIZE_WALLET // TODO what is this?
                          "Sign transaction",
#else
                          NULL,
#endif
                          tx_review_choice);
    }
    return 0;
}
