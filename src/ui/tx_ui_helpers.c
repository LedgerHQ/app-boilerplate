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

#include <string.h>

#include "tx_ui_helpers.h"
#include "ui_utils.h"
#include "ui_constants.h"
#include "cardano_swo.h"
#include "addressUtils/addressUtilsShelley.h"
#include "addressUtils/bip44.h"
#include "addressUtils/bech32.h"
#include "utils/assert.h"
#include "utils/textUtils.h"
#include "memory/mem.h"

static char *ui_alloc_temp(size_t size) {
    if (size == 0) {
        return NULL;
    }
    char *tmp = (char *) app_mem_alloc(size);
    if (tmp != NULL) {
        explicit_bzero(tmp, size);
    }
    return tmp;
}

static int ui_add_pair_or_fail(const char *label, char *tmp_buf) {
    if (!ui_pairs_add(label, tmp_buf)) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    return SWO_SUCCESS;
}

/**
 * Display credential (key path, key hash, or script hash) with context-specific bech32 prefix
 *
 * This follows the old app's pattern of displaying credentials appropriately based on their type:
 * - KEY_PATH: Shows the BIP44 derivation path
 * - KEY_HASH: Shows bech32-encoded key hash with specified prefix
 * - SCRIPT_HASH: Shows bech32-encoded script hash with specified prefix
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
int displayCredential(const ext_credential_t *credential,
                     const char *keyPathLabel,
                     const char *keyHashLabel,
                     const char *keyHashPrefix,
                     const char *scriptHashLabel,
                     const char *scriptHashPrefix) {
    LEDGER_ASSERT(credential != NULL, "NULL credential");
    LEDGER_ASSERT(keyPathLabel != NULL, "NULL keyPathLabel");
    LEDGER_ASSERT(keyHashLabel != NULL, "NULL keyHashLabel");
    LEDGER_ASSERT(keyHashPrefix != NULL, "NULL keyHashPrefix");
    LEDGER_ASSERT(scriptHashLabel != NULL, "NULL scriptHashLabel");
    LEDGER_ASSERT(scriptHashPrefix != NULL, "NULL scriptHashPrefix");

    int status;

    switch (credential->type) {
        case EXT_CREDENTIAL_KEY_PATH: {
            // Display BIP44 path
            char *path_tmp = ui_alloc_temp(MAX_BIP44_PATH_STRING_LENGTH + 1);
            if (path_tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            if (!format_bip44_path(&credential->keyPath, path_tmp, MAX_BIP44_PATH_STRING_LENGTH + 1)) {
                return SWO_TX_PARSING_FAIL;
            }
            status = ui_add_pair_or_fail(keyPathLabel, path_tmp);
            if (status != SWO_SUCCESS) {
                return status;
            }
            break;
        }
        case EXT_CREDENTIAL_KEY_HASH: {
            // Display key hash as bech32
            char *keyhash_tmp = ui_alloc_temp(MAX_BECH32_STRING_LENGTH);
            if (keyhash_tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            if (!format_bech32(keyHashPrefix,
                              credential->keyHash,
                              ADDRESS_KEY_HASH_LENGTH,
                              keyhash_tmp,
                              MAX_BECH32_STRING_LENGTH)) {
                return SWO_TX_PARSING_FAIL;
            }
            status = ui_add_pair_or_fail(keyHashLabel, keyhash_tmp);
            if (status != SWO_SUCCESS) {
                return status;
            }
            break;
        }
        case EXT_CREDENTIAL_SCRIPT_HASH: {
            // Display script hash as bech32
            char *scripthash_tmp = ui_alloc_temp(MAX_BECH32_STRING_LENGTH);
            if (scripthash_tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            if (!format_bech32(scriptHashPrefix,
                              credential->scriptHash,
                              SCRIPT_HASH_LENGTH,
                              scripthash_tmp,
                              MAX_BECH32_STRING_LENGTH)) {
                return SWO_TX_PARSING_FAIL;
            }
            status = ui_add_pair_or_fail(scriptHashLabel, scripthash_tmp);
            if (status != SWO_SUCCESS) {
                return status;
            }
            break;
        }
        default:
            return SWO_TX_PARSING_FAIL;
    }

    return SWO_SUCCESS;
}

/**
 * Display DRep (Delegated Representative) with appropriate formatting
 *
 * Handles all DRep types:
 * - KEY_PATH: Shows the BIP44 derivation path
 * - KEY_HASH: Shows bech32-encoded key hash with "drep" prefix
 * - SCRIPT_HASH: Shows bech32-encoded script hash with "drep" prefix
 * - ABSTAIN: Shows "Abstain"
 * - NO_CONFIDENCE: Shows "No Confidence"
 *
 * @param[in]  drep             DRep to display (type + data)
 * @param[in]  label            Label for the UI pair (e.g., "DRep")
 *
 * @return SWO_SUCCESS on success, or error code on failure
 */
int displayDRep(const ext_drep_t *drep, const char *label) {
    LEDGER_ASSERT(drep != NULL, "NULL drep");
    LEDGER_ASSERT(label != NULL, "NULL label");

    char *tmp = NULL;
    int status;

    switch (drep->type) {
        case EXT_DREP_KEY_PATH: {
            // Display BIP44 path
            tmp = ui_alloc_temp(MAX_BIP44_PATH_STRING_LENGTH + 1);
            if (tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            if (!format_bip44_path(&drep->keyPath, tmp, MAX_BIP44_PATH_STRING_LENGTH + 1)) {
                return SWO_TX_PARSING_FAIL;
            }
            break;
        }
        case EXT_DREP_KEY_HASH: {
            // Display key hash as bech32 with "drep" prefix
            tmp = ui_alloc_temp(MAX_BECH32_STRING_LENGTH);
            if (tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            if (!format_bech32("drep",
                              drep->keyHash,
                              ADDRESS_KEY_HASH_LENGTH,
                              tmp,
                              MAX_BECH32_STRING_LENGTH)) {
                return SWO_TX_PARSING_FAIL;
            }
            break;
        }
        case EXT_DREP_SCRIPT_HASH: {
            // Display script hash as bech32 with "drep" prefix
            tmp = ui_alloc_temp(MAX_BECH32_STRING_LENGTH);
            if (tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            if (!format_bech32("drep",
                              drep->scriptHash,
                              SCRIPT_HASH_LENGTH,
                              tmp,
                              MAX_BECH32_STRING_LENGTH)) {
                return SWO_TX_PARSING_FAIL;
            }
            break;
        }
        case EXT_DREP_ABSTAIN: {
            // Display special case: Abstain
            tmp = ui_alloc_temp(16);  // "Abstain" + null
            if (tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            snprintf(tmp, 16, "Abstain");
            break;
        }
        case EXT_DREP_NO_CONFIDENCE: {
            // Display special case: No Confidence
            tmp = ui_alloc_temp(32);  // "No Confidence" + null
            if (tmp == NULL) {
                return SWO_INSUFFICIENT_MEMORY;
            }
            snprintf(tmp, 32, "No Confidence");
            break;
        }
        default:
            return SWO_TX_PARSING_FAIL;
    }

    status = ui_add_pair_or_fail(label, tmp);
    return status;
}

bool formatRewardAddressFromCredential(uint8_t networkId,
                                      const ext_credential_t *credential,
                                      char *buffer,
                                      size_t buffer_size) {
    LEDGER_ASSERT(credential != NULL, "NULL credential");
    LEDGER_ASSERT(buffer != NULL, "NULL buffer");

    uint8_t reward_addr_bytes[REWARD_ACCOUNT_LENGTH];
    size_t reward_addr_len = 0;

    switch (credential->type) {
        case EXT_CREDENTIAL_KEY_PATH:
            reward_addr_len = constructRewardAddressFromKeyPath(
                &credential->keyPath,
                networkId,
                reward_addr_bytes,
                sizeof(reward_addr_bytes)
            );
            break;
        case EXT_CREDENTIAL_KEY_HASH:
            reward_addr_len = constructRewardAddressFromHash(
                networkId,
                REWARD_HASH_SOURCE_KEY,
                credential->keyHash,
                ADDRESS_KEY_HASH_LENGTH,
                reward_addr_bytes,
                sizeof(reward_addr_bytes)
            );
            break;
        case EXT_CREDENTIAL_SCRIPT_HASH:
            reward_addr_len = constructRewardAddressFromHash(
                networkId,
                REWARD_HASH_SOURCE_SCRIPT,
                credential->scriptHash,
                SCRIPT_HASH_LENGTH,
                reward_addr_bytes,
                sizeof(reward_addr_bytes)
            );
            break;
        default:
            return false;
    }

    if (reward_addr_len == 0) {
        return false;
    }

    return format_address_human_readable(
        reward_addr_bytes,
        reward_addr_len,
        buffer,
        buffer_size
    );
}

int displayAnchorIfPresent(const anchor_t *anchor) {
    LEDGER_ASSERT(anchor != NULL, "NULL anchor");

    if (!anchor->isIncluded) {
        return SWO_SUCCESS;
    }

    int status;

    // Display anchor URL
    char *anchor_url_tmp = ui_alloc_temp(ANCHOR_URL_LENGTH_MAX + 1);
    if (anchor_url_tmp == NULL) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    memcpy(anchor_url_tmp,
           anchor->url,
           anchor->urlLength);
    anchor_url_tmp[anchor->urlLength] = '\0';
    status = ui_add_pair_or_fail("Anchor URL", anchor_url_tmp);
    if (status != SWO_SUCCESS) {
        return status;
    }

    // Display anchor hash
    char *anchor_hash_tmp = ui_alloc_temp(MAX_BECH32_STRING_LENGTH);
    if (anchor_hash_tmp == NULL) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    if (!format_bech32("anchor",
                      anchor->hash,
                      ANCHOR_HASH_LENGTH,
                      anchor_hash_tmp,
                      MAX_BECH32_STRING_LENGTH)) {
        return SWO_TX_PARSING_FAIL;
    }
    status = ui_add_pair_or_fail("Anchor hash", anchor_hash_tmp);
    if (status != SWO_SUCCESS) {
        return status;
    }

    return SWO_SUCCESS;
}

int displayDeposit(uint64_t deposit, const char *label) {
    LEDGER_ASSERT(label != NULL, "NULL label");

    char *deposit_tmp = ui_alloc_temp(MAX_ADA_AMOUNT_STRING_LENGTH);
    if (deposit_tmp == NULL) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    bool deposit_formatted = str_formatAdaAmount(
        deposit,
        deposit_tmp,
        MAX_ADA_AMOUNT_STRING_LENGTH
    );
    ASSERT(deposit_formatted);
    return ui_add_pair_or_fail(label, deposit_tmp);
}

int displayPoolKeyHash(const uint8_t *poolKeyHash, const char *label) {
    LEDGER_ASSERT(poolKeyHash != NULL, "NULL poolKeyHash");
    LEDGER_ASSERT(label != NULL, "NULL label");

    char *pool_tmp = ui_alloc_temp(MAX_BECH32_STRING_LENGTH);
    if (pool_tmp == NULL) {
        return SWO_INSUFFICIENT_MEMORY;
    }
    if (!format_bech32("pool",
                      poolKeyHash,
                      POOL_KEY_HASH_LENGTH,
                      pool_tmp,
                      MAX_BECH32_STRING_LENGTH)) {
        return SWO_TX_PARSING_FAIL;
    }
    return ui_add_pair_or_fail(label, pool_tmp);
}

int displayRewardAccountFromCredential(uint8_t networkId, const ext_credential_t *credential) {
    LEDGER_ASSERT(credential != NULL, "NULL credential");

    // Build the reward address first (needed for all credential types)
    char reward_addr_buf[MAX_HUMAN_ADDRESS_LENGTH];
    if (!formatRewardAddressFromCredential(networkId, credential, reward_addr_buf, sizeof(reward_addr_buf))) {
        return SWO_TX_PARSING_FAIL;
    }

    char *value_tmp = NULL;

    if (credential->type == EXT_CREDENTIAL_KEY_PATH) {
        // For KEY_PATH: combine path and address in value
        char path_buf[MAX_BIP44_PATH_STRING_LENGTH + 1];
        if (!format_bip44_path(&credential->keyPath, path_buf, sizeof(path_buf))) {
            return SWO_TX_PARSING_FAIL;
        }

        // Allocate and combine path and address with space separator
        value_tmp = ui_alloc_temp(MAX_HUMAN_ADDRESS_LENGTH + MAX_BIP44_PATH_STRING_LENGTH + 2);
        if (value_tmp == NULL) {
            return SWO_INSUFFICIENT_MEMORY;
        }
        snprintf(value_tmp,
                MAX_HUMAN_ADDRESS_LENGTH + MAX_BIP44_PATH_STRING_LENGTH + 2,
                "%s %s",
                path_buf,
                reward_addr_buf);
    } else {
        // For KEY_HASH/SCRIPT_HASH: show just the address
        value_tmp = ui_alloc_temp(MAX_HUMAN_ADDRESS_LENGTH);
        if (value_tmp == NULL) {
            return SWO_INSUFFICIENT_MEMORY;
        }
        snprintf(value_tmp, MAX_HUMAN_ADDRESS_LENGTH, "%s", reward_addr_buf);
    }

    // Use same label for all types
    return ui_add_pair_or_fail("Reward account", value_tmp);
}

const char *getCertificateTypeName(certificate_type_t type) {
    switch (type) {
        case CERTIFICATE_STAKE_REGISTRATION:
            return "Stake Registration";
        case CERTIFICATE_STAKE_DEREGISTRATION:
            return "Stake Deregistration";
        case CERTIFICATE_STAKE_DELEGATION:
            return "Stake Delegation";
        case CERTIFICATE_STAKE_POOL_RETIREMENT:
            return "Pool Retirement";
        case CERTIFICATE_STAKE_REGISTRATION_CONWAY:
            return "Stake Registration (Conway)";
        case CERTIFICATE_STAKE_DEREGISTRATION_CONWAY:
            return "Stake Deregistration (Conway)";
        case CERTIFICATE_VOTE_DELEGATION:
            return "Vote Delegation";
        case CERTIFICATE_AUTHORIZE_COMMITTEE_HOT:
            return "Committee Authorization";
        case CERTIFICATE_RESIGN_COMMITTEE_COLD:
            return "Committee Resignation";
        case CERTIFICATE_DREP_REGISTRATION:
            return "DRep Registration";
        case CERTIFICATE_DREP_DEREGISTRATION:
            return "DRep Deregistration";
        case CERTIFICATE_DREP_UPDATE:
            return "DRep Update";
        default:
            return "Unknown";
    }
}
