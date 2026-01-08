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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "transaction/tx_credential_types.h"
#include "transaction/tx_certificate_types.h"

/**
 * Construct reward address from a credential and format to human-readable string
 *
 * Handles all credential types (KEY_PATH, KEY_HASH, SCRIPT_HASH) by converting
 * them to reward account addresses and formatting as human-readable strings.
 *
 * @param[in]  networkId      Network ID for address construction
 * @param[in]  credential     Credential to convert (type + data)
 * @param[out] buffer         Output buffer for formatted address string
 * @param[in]  buffer_size    Size of output buffer
 *
 * @return true if formatting succeeded, false on error
 */
bool formatRewardAddressFromCredential(uint8_t networkId,
                                      const ext_credential_t *credential,
                                      char *buffer,
                                      size_t buffer_size);

/**
 * Add credential (key path, key hash, or script hash) to UI pairs with context-specific bech32 prefix
 *
 * @param[in]  credential       Credential to display (type + data)
 * @param[in]  keyPathLabel     Label for KEY_PATH type (e.g., "Stake key")
 * @param[in]  keyHashLabel     Label for KEY_HASH type (e.g., "Stake key hash")
 * @param[in]  keyHashPrefix    Bech32 prefix for KEY_HASH (e.g., "stake_vkh")
 * @param[in]  scriptHashLabel  Label for SCRIPT_HASH type (e.g., "Stake script hash")
 * @param[in]  scriptHashPrefix Bech32 prefix for SCRIPT_HASH (e.g., "script")
 *
 * @return SWO_SUCCESS on success, or error code on failure
 */
int addCredentialUIPairs(const ext_credential_t *credential,
                        const char *keyPathLabel,
                        const char *keyHashLabel,
                        const char *keyHashPrefix,
                        const char *scriptHashLabel,
                        const char *scriptHashPrefix);

/**
 * Add voter (key path, key hash, or script hash) to UI pairs with appropriate labels and prefixes
 *
 * @param[in]  voter            Voter to display (type + data)
 *
 * @return SWO_SUCCESS on success, or error code on failure
 */
int addVoterUIPairs(const ext_voter_t *voter);

/**
 * Add DRep (Delegated Representative) to UI pairs with appropriate formatting
 *
 * @param[in]  drep             DRep to display (type + data)
 * @param[in]  label            Label for the UI pair (e.g., "DRep")
 *
 * @return SWO_SUCCESS on success, or error code on failure
 */
int addDRepUIPairs(const ext_drep_t *drep, const char *label);

/**
 * Add anchor (URL + hash) to UI pairs if present
 *
 * @param[in]  anchor        Anchor structure to display
 *
 * @return SWO_SUCCESS on success, or error code on failure
 */
int addAnchorUIPairs(const anchor_t *anchor);

/**
 * Add deposit amount to UI pair
 *
 * @param[in]  deposit       Deposit amount in lovelace
 * @param[in]  label         Label for the UI pair (default: "Deposit")
 *
 * @return SWO_SUCCESS on success, or error code on failure
 */
int addDepositUIPairs(uint64_t deposit, const char *label);

/**
 * Add pool key hash to UI pairs in bech32 format with "pool" prefix
 *
 * @param[in]  poolKeyHash   28-byte pool key hash
 * @param[in]  label         Label for the UI pair (e.g., "Pool", "Pool ID")
 *
 * @return SWO_SUCCESS on success, or error code on failure
 */
int addPoolKeyHashUIPairs(const uint8_t *poolKeyHash, const char *label);

/**
 * Add reward account from credential to UI pairs for withdrawals
 *
 * @param[in]  networkId     Network ID for reward address construction
 * @param[in]  credential    Credential to display (type + data)
 *
 * @return SWO_SUCCESS on success, or error code on failure
 */
int addRewardAccountFromCredentialUIPairs(uint8_t networkId, const ext_credential_t *credential);

/**
 * Add reward address derived from a credential to UI pairs
 *
 * @param[in]  networkId     Network ID for reward address construction
 * @param[in]  credential    Credential to display (type + data)
 * @param[in]  label         Label for the UI pair
 *
 * @return SWO_SUCCESS on success, or error code on failure
 */
int addRewardAddressFromCredentialUIPairs(uint8_t networkId,
                                          const ext_credential_t *credential,
                                          const char *label);

/**
 * Add reward account address to UI pairs (pool reward account)
 *
 * @param[in]  networkId      Network ID for reward address construction
 * @param[in]  rewardAccount  Reward account to display (type + data)
 * @param[in]  label          Label for the UI pair
 *
 * @return SWO_SUCCESS on success, or error code on failure
 */
int addRewardAccountUIPairs(uint8_t networkId,
                            const reward_account_t *rewardAccount,
                            const char *label);

/**
 * Get human-readable name for a certificate type
 *
 * @param[in]  type          Certificate type to get name for
 *
 * @return Pointer to constant string with certificate type name
 */
const char *getCertificateTypeName(certificate_type_t type);
