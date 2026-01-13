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
#include "transaction/tx.h"

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
/**
 * Add credential (key path, key hash, or script hash) to UI pairs with context-specific bech32 prefix
 *
 * On error, error status is set via ui_set_error_status() and can be retrieved with ui_get_error_status()
 *
 * @param[in]  credential       Credential to display (type + data)
 * @param[in]  keyPathLabel     Label for KEY_PATH type (e.g., "Stake key")
 * @param[in]  keyHashLabel     Label for KEY_HASH type (e.g., "Stake key hash")
 * @param[in]  keyHashPrefix    Bech32 prefix for KEY_HASH (e.g., "stake_vkh")
 * @param[in]  scriptHashLabel  Label for SCRIPT_HASH type (e.g., "Stake script hash")
 * @param[in]  scriptHashPrefix Bech32 prefix for SCRIPT_HASH (e.g., "script")
 */
void addCredentialUIPairs(const ext_credential_t *credential,
                        const char *keyPathLabel,
                        const char *keyHashLabel,
                        const char *keyHashPrefix,
                        const char *scriptHashLabel,
                        const char *scriptHashPrefix);

/**
 * Add voter (key path, key hash, or script hash) to UI pairs with appropriate labels and prefixes
 *
 * On error, error status is set via ui_set_error_status() and can be retrieved with ui_get_error_status()
 *
 * @param[in]  voter            Voter to display (type + data)
 */
void addVoterUIPairs(const ext_voter_t *voter);

/**
 * Add DRep (Delegated Representative) to UI pairs with appropriate formatting
 *
 * On error, error status is set via ui_set_error_status() and can be retrieved with ui_get_error_status()
 *
 * @param[in]  drep             DRep to display (type + data)
 * @param[in]  label            Label for the UI pair (e.g., "DRep")
 */
void addDRepUIPairs(const ext_drep_t *drep, const char *label);

/**
 * Add anchor (URL + hash) to UI pairs if present
 *
 * On error, error status is set via ui_set_error_status() and can be retrieved with ui_get_error_status()
 *
 * @param[in]  anchor        Anchor structure to display
 */
void addAnchorUIPairs(const anchor_t *anchor);

/**
 * Add withdrawal information from credential to UI pairs
 *
 * @param[in]  networkId     Network ID for reward address construction
 * @param[in]  credential    Credential to display (type + data)
 *
 * On error, error status is set via ui_set_error_status() and can be retrieved with ui_get_error_status()
 */
void addWithdrawalUIPairs(uint8_t networkId, const ext_credential_t *credential);

/**
 * Add certificate UI pairs based on certificate type and security policy
 *
 * On error, error status is set via ui_set_error_status() and can be retrieved with ui_get_error_status()
 *
 * @param[in] certificate_data   Certificate data to display
 * @param[in] txSigningMode      Transaction signing mode
 */
void addCertificateUIPairs(const certificate_data_t* certificate_data, sign_tx_signingmode_t txSigningMode);

/**
 * Add payment credential UI pair for device-owned address
 *
 * Adds one UI pair showing the payment credential (either key path or script hash)
 *
 * On error, error status is set via ui_set_error_status() and can be retrieved with ui_get_error_status()
 *
 * @param[in]  addressParams  Address parameters containing payment info
 */
void addPaymentInfoUIPair(const addressParams_t* addressParams);

/**
 * Add staking credential UI pair for device-owned address
 *
 * Adds one UI pair showing the staking credential (path/hash/script/pointer/warning)
 *
 * On error, error status is set via ui_set_error_status() and can be retrieved with ui_get_error_status()
 *
 * @param[in]  addressParams  Address parameters containing staking info
 */
void addStakingInfoUIPair(const addressParams_t* addressParams);
