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
#pragma once

#include <stdint.h>
#include <stdbool.h>

// UI pair count constants for transaction elements.
// These define how many UI pairs each transaction element contributes when displayed.
// CRITICAL: These must match exactly with the actual UI_ADD_* calls in tx_ui_format.c.
// Ordered by CBOR transaction body keys (0, 1, 2, 3, 4, 5, 7, 8, 9, 11, 13, 14, 17, 18, 21, 22, ...)
#define UI_PAIRS_INPUT 1                         // key 0: "Input"
#define UI_PAIRS_OUTPUT_BASE 3                   // key 1: "Output", "Address", "Amount"
#define UI_PAIRS_OUTPUT_DEVICE_OWNED 2           // key 1: "Payment info", "Staking info" (additional if device-owned)
#define UI_PAIRS_OUTPUT_DATUM 1                  // key 1: "Datum hash" or "Inline datum" (if present and shown)
#define UI_PAIRS_OUTPUT_REF_SCRIPT 1             // key 1: "Reference script" (if present and shown)
#define UI_PAIRS_FEE 1                           // key 2: "Fee"
#define UI_PAIRS_TTL 1                           // key 3: "TTL"
#define UI_PAIRS_CERTIFICATE_STAKE_REGISTRATION 2              // key 4: "Certificate", credential
#define UI_PAIRS_CERTIFICATE_STAKE_DEREGISTRATION 2            // key 4: "Certificate", credential
#define UI_PAIRS_CERTIFICATE_STAKE_DELEGATION 3                // key 4: "Certificate", credential, "Pool"
#define UI_PAIRS_CERTIFICATE_STAKE_REGISTRATION_CONWAY 3       // key 4: "Certificate", credential, "Deposit"
#define UI_PAIRS_CERTIFICATE_STAKE_DEREGISTRATION_CONWAY 3     // key 4: "Certificate", credential, "Deposit"
#define UI_PAIRS_CERTIFICATE_POOL_RETIREMENT 3                 // key 4: "Certificate", "Pool ID", "Retirement epoch"
#define UI_PAIRS_CERTIFICATE_POOL_REGISTRATION_BASE 1          // key 4: "Certificate" (pool registration base)
#define UI_PAIRS_POOL_ID 1                                     // key 4: "Pool ID"
#define UI_PAIRS_POOL_VRF_KEY 1                                // key 4: "VRF key hash"
#define UI_PAIRS_POOL_FIXED 3                                  // key 4: "Pledge", "Cost", "Profit margin"
#define UI_PAIRS_POOL_REWARD_ACCOUNT 1                         // key 4: "Pool reward address"
#define UI_PAIRS_POOL_OWNER 1                                  // key 4: "Owner reward address"
#define UI_PAIRS_POOL_NO_OWNERS 1                              // key 4: "Pool owners: None"
#define UI_PAIRS_POOL_RELAY_HEADER 1                           // key 4: "Relay #N"
#define UI_PAIRS_POOL_RELAY_IPV4 1                             // key 4: "IPv4"
#define UI_PAIRS_POOL_RELAY_IPV6 1                             // key 4: "IPv6"
#define UI_PAIRS_POOL_RELAY_PORT 1                             // key 4: "Port"
#define UI_PAIRS_POOL_RELAY_DNS 1                              // key 4: "DNS name" or "SRV DNS"
#define UI_PAIRS_POOL_NO_RELAYS 1                              // key 4: "Pool relays: None"
#define UI_PAIRS_POOL_METADATA 2                               // key 4: "Pool metadata url", "Pool metadata hash"
#define UI_PAIRS_POOL_NO_METADATA 1                            // key 4: "Metadata: none (anonymous pool)"
#define UI_PAIRS_CERTIFICATE_VOTE_DELEGATION 3                 // key 4: "Certificate", voter credential, DRep
#define UI_PAIRS_CERTIFICATE_AUTHORIZE_COMMITTEE_HOT 3         // key 4: "Certificate", cold credential, hot credential
#define UI_PAIRS_CERTIFICATE_RESIGN_COMMITTEE_COLD 2           // key 4: "Certificate", cold credential (+ anchor if included)
#define UI_PAIRS_CERTIFICATE_DREP_REGISTRATION 3               // key 4: "Certificate", DRep credential, "Deposit" (+ anchor if included)
#define UI_PAIRS_CERTIFICATE_DREP_DEREGISTRATION 3             // key 4: "Certificate", DRep credential, "Deposit"
#define UI_PAIRS_CERTIFICATE_DREP_UPDATE 2                     // key 4: "Certificate", DRep credential (+ anchor if included)
#define UI_PAIRS_WITHDRAWAL_KEY_PATH 3           // key 5: "Withdrawal", "Withdrawal path", "Withdrawal" (reward addr)
#define UI_PAIRS_WITHDRAWAL_OTHER 2              // key 5: "Withdrawal", "Withdrawal" (reward addr)
#define UI_PAIRS_AUXILIARY_DATA_HASH 1           // key 7: "Auxiliary data hash"
#define UI_PAIRS_VALIDITY_INTERVAL_START 1       // key 8: "Validity interval start"
#define UI_PAIRS_MINT_SUMMARY 1                  // key 9: "Mint" (summary line)
#define UI_PAIRS_TOKEN 2                         // Token pair: "Asset fingerprint", "Token/Mint amount" (used in outputs, mint, collateral output)
#define UI_PAIRS_SCRIPT_DATA_HASH 1              // key 11: "Script data hash"
#define UI_PAIRS_COLLATERAL_INPUT 1              // key 13: "Coll input"
#define UI_PAIRS_REQUIRED_SIGNER 1               // key 14: "Required signer"
#define UI_PAIRS_COLLATERAL_OUTPUT_ADDRESS 1     // key 16: "Collateral address"
#define UI_PAIRS_COLLATERAL_OUTPUT_DEVICE_OWNED 2 // key 16: "Payment info", "Staking info" (additional if device-owned)
#define UI_PAIRS_COLLATERAL_OUTPUT_AMOUNT 1      // key 16: "Collateral amount" (if shown)
#define UI_PAIRS_TOTAL_COLLATERAL 1              // key 17: "Total collateral"
#define UI_PAIRS_REFERENCE_INPUT 1               // key 18: "Ref input"
#define UI_PAIRS_VOTER 1                         // key 19: Voter credential (Committee hot key, DRep key, SPO key)
#define UI_PAIRS_VOTE 3                          // key 19: "Gov action tx hash", "Gov action index", "Vote"
#define UI_PAIRS_ANCHOR 2                        // key 19: "Anchor URL", "Anchor hash" (if anchor included)
#define UI_PAIRS_TREASURY 1                      // key 21: "Treasury"
#define UI_PAIRS_DONATION 1                      // key 22: "Donation"
#define UI_PAIRS_TX_HASH 1                       // Transaction hash display

/**
 * Plan for UI pair consumption and display constraints when preparing a transaction review.
 *
 * This structure is populated during transaction validation (tx_validate_and_compute_hash)
 * and consumed during UI formatting (ui_prepare_transaction_review).
 */
typedef struct {
    uint32_t pair_count;  /// Number of nbgl_contentTagValue pairs required for display

    // TODO: Detect and track transaction elements with excessive length
    // Some transaction elements are not length-limited by CDDL (e.g., metadata URLs,
    // DNS names in relays, inline datums, reference scripts). We should:
    // - Track whether any element exceeds reasonable display limits during validation
    // - Set a flag or store max element size encountered
    // - Use this during UI formatting to decide between full display vs truncation/streaming
    // - Consider if we need to enforce hard limits for security (DoS via huge fields)
    bool has_excessive_length_element;  // TODO: Implement detection during validation
} tx_ui_plan_t;
