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
#include <stdio.h>

#include "tx_ui_helpers.h"
#include "ui_utils.h"
#include "ui_constants.h"
#include "ui_formatters.h"
#include "cardano_swo.h"
#include "addressUtils/addressUtilsShelley.h"
#include "addressUtils/bip44.h"
#include "addressUtils/bech32.h"
#include "utils/assert.h"
#include "memory/mem.h"

void addCredentialUIPairs(const ext_credential_t *credential,
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

    switch (credential->type) {
        case EXT_CREDENTIAL_KEY_PATH: {
            UI_ADD_FORMAT1(keyPathLabel, MAX_BIP44_PATH_STRING_LENGTH, format_bip44_path, &credential->keyPath);
            break;
        }
        case EXT_CREDENTIAL_KEY_HASH: {
            UI_ADD_FORMAT3(keyHashLabel, MAX_BECH32_STRING_LENGTH, format_bech32, keyHashPrefix, credential->keyHash, ADDRESS_KEY_HASH_LENGTH);
            break;
        }
        case EXT_CREDENTIAL_SCRIPT_HASH: {
            UI_ADD_FORMAT3(scriptHashLabel, MAX_BECH32_STRING_LENGTH, format_bech32, scriptHashPrefix, credential->scriptHash, SCRIPT_HASH_LENGTH);
            break;
        }
        default:
            LEDGER_ASSERT(false, "Unknown credential type");
    }
}

void addVoterUIPairs(const ext_voter_t *voter) {
    LEDGER_ASSERT(voter != NULL, "NULL voter");
    ext_credential_t voter_credential = {0};

    switch (voter->type) {
        case EXT_VOTER_COMMITTEE_HOT_KEY_PATH:
            voter_credential.type = EXT_CREDENTIAL_KEY_PATH;
            voter_credential.keyPath = voter->keyPath;
            addCredentialUIPairs(&voter_credential,
                                          UI_STATIC_LABEL("Committee hot key"),
                                          UI_STATIC_LABEL(""), UI_STATIC_LABEL(""), UI_STATIC_LABEL(""), UI_STATIC_LABEL("")); // Path only needs first label
            break;
        case EXT_VOTER_COMMITTEE_HOT_KEY_HASH:
            voter_credential.type = EXT_CREDENTIAL_KEY_HASH;
            memcpy(voter_credential.keyHash, voter->keyHash, sizeof(voter_credential.keyHash));
            addCredentialUIPairs(&voter_credential,
                                          UI_STATIC_LABEL(""),
                                          UI_STATIC_LABEL("Committee hot key hash"),
                                          UI_STATIC_LABEL("cc_hot"),
                                          UI_STATIC_LABEL(""), UI_STATIC_LABEL(""));
            break;
        case EXT_VOTER_COMMITTEE_HOT_SCRIPT_HASH:
            voter_credential.type = EXT_CREDENTIAL_SCRIPT_HASH;
            memcpy(voter_credential.scriptHash, voter->scriptHash, sizeof(voter_credential.scriptHash));
            addCredentialUIPairs(&voter_credential,
                                          UI_STATIC_LABEL(""), UI_STATIC_LABEL(""), UI_STATIC_LABEL(""),
                                          UI_STATIC_LABEL("Committee hot script hash"),
                                          UI_STATIC_LABEL("cc_hot"));
            break;
        case EXT_VOTER_DREP_KEY_PATH:
            voter_credential.type = EXT_CREDENTIAL_KEY_PATH;
            voter_credential.keyPath = voter->keyPath;
            addCredentialUIPairs(&voter_credential,
                                          UI_STATIC_LABEL("DRep key"),
                                          UI_STATIC_LABEL(""), UI_STATIC_LABEL(""), UI_STATIC_LABEL(""), UI_STATIC_LABEL(""));
            break;
        case EXT_VOTER_DREP_KEY_HASH:
            voter_credential.type = EXT_CREDENTIAL_KEY_HASH;
            memcpy(voter_credential.keyHash, voter->keyHash, sizeof(voter_credential.keyHash));
            addCredentialUIPairs(&voter_credential,
                                          UI_STATIC_LABEL(""),
                                          UI_STATIC_LABEL("DRep key hash"),
                                          UI_STATIC_LABEL("drep"),
                                          UI_STATIC_LABEL(""), UI_STATIC_LABEL(""));
            break;
        case EXT_VOTER_DREP_SCRIPT_HASH:
            voter_credential.type = EXT_CREDENTIAL_SCRIPT_HASH;
            memcpy(voter_credential.scriptHash, voter->scriptHash, sizeof(voter_credential.scriptHash));
            addCredentialUIPairs(&voter_credential,
                                          UI_STATIC_LABEL(""), UI_STATIC_LABEL(""), UI_STATIC_LABEL(""),
                                          UI_STATIC_LABEL("DRep script hash"),
                                          UI_STATIC_LABEL("drep"));
            break;
        case EXT_VOTER_STAKE_POOL_KEY_PATH:
            voter_credential.type = EXT_CREDENTIAL_KEY_PATH;
            voter_credential.keyPath = voter->keyPath;
            addCredentialUIPairs(&voter_credential,
                                          UI_STATIC_LABEL("Stake pool key"),
                                          UI_STATIC_LABEL(""), UI_STATIC_LABEL(""), UI_STATIC_LABEL(""), UI_STATIC_LABEL(""));
            break;
        case EXT_VOTER_STAKE_POOL_KEY_HASH:
            voter_credential.type = EXT_CREDENTIAL_KEY_HASH;
            memcpy(voter_credential.keyHash, voter->keyHash, sizeof(voter_credential.keyHash));
            addCredentialUIPairs(&voter_credential,
                                          UI_STATIC_LABEL(""),
                                          UI_STATIC_LABEL("Stake pool key hash"),
                                          UI_STATIC_LABEL("pool"),
                                          UI_STATIC_LABEL(""), UI_STATIC_LABEL(""));
            break;
        default:
            LEDGER_ASSERT(false, "Unknown voter type");
            break;
    }
}

void addDRepUIPairs(const ext_drep_t *drep, const char *label) {
    LEDGER_ASSERT(drep != NULL, "NULL drep");
    LEDGER_ASSERT(label != NULL, "NULL label");

    switch (drep->type) {
        case EXT_DREP_KEY_PATH: {
            UI_ADD_FORMAT1(label, MAX_BIP44_PATH_STRING_LENGTH, format_bip44_path, &drep->keyPath);
            break;
        }
        case EXT_DREP_KEY_HASH: {
            UI_ADD_FORMAT3(label, MAX_BECH32_STRING_LENGTH, format_bech32, "drep", drep->keyHash, ADDRESS_KEY_HASH_LENGTH);
            break;
        }
        case EXT_DREP_SCRIPT_HASH: {
            UI_ADD_FORMAT3(label, MAX_BECH32_STRING_LENGTH, format_bech32, "drep", drep->scriptHash, SCRIPT_HASH_LENGTH);
            break;
        }
        case EXT_DREP_ABSTAIN:
        case EXT_DREP_NO_CONFIDENCE: {
            UI_ADD_FORMAT1(label, MAX_DREP_OPTION_LENGTH, format_constant_drep, drep->type);
            break;
        }
        default:
            LEDGER_ASSERT(false, "Unknown DRep type");
    }
}

void addAnchorUIPairs(const anchor_t *anchor) {
    LEDGER_ASSERT(anchor != NULL, "NULL anchor");

    if (!anchor->isIncluded) {
        return;
    }

    UI_ADD_FORMAT2(UI_STATIC_LABEL("Anchor URL"), MAX_ANCHOR_URL_LENGTH, format_url, anchor->url, anchor->urlLength);
    UI_ADD_FORMAT3(UI_STATIC_LABEL("Anchor hash"), MAX_BECH32_STRING_LENGTH, format_bech32, "anchor", anchor->hash, ANCHOR_HASH_LENGTH);
}

void addDepositUIPairs(uint64_t deposit, const char *label) {
    LEDGER_ASSERT(label != NULL, "NULL label");

    UI_ADD_FORMAT1(label, MAX_ADA_AMOUNT_STRING_LENGTH, format_ada_amount, deposit);
}

void addPoolKeyHashUIPairs(const uint8_t *poolKeyHash, const char *label) {
    LEDGER_ASSERT(poolKeyHash != NULL, "NULL poolKeyHash");
    LEDGER_ASSERT(label != NULL, "NULL label");

    UI_ADD_FORMAT3(label, MAX_BECH32_STRING_LENGTH, format_bech32, "pool", poolKeyHash, POOL_KEY_HASH_LENGTH);
}

void addRewardAccountFromCredentialUIPairs(uint8_t networkId, const ext_credential_t *credential) {
    LEDGER_ASSERT(credential != NULL, "NULL credential");

    switch (credential->type) {
        case EXT_CREDENTIAL_KEY_PATH: {
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Withdrawal path"), MAX_BIP44_PATH_STRING_LENGTH, format_bip44_path, &credential->keyPath);
            break;
        }
        case EXT_CREDENTIAL_KEY_HASH:
        case EXT_CREDENTIAL_SCRIPT_HASH: {
            break;
        }
        default:
            LEDGER_ASSERT(false, "Unknown credential type");
    }

    UI_ADD_FORMAT2(UI_STATIC_LABEL("Withdrawal"),
                   MAX_HUMAN_ADDRESS_LENGTH,
                   format_reward_account_from_credential,
                   networkId,
                   credential);
}

void addPaymentInfoUIPair(const addressParams_t* addressParams) {
    switch (determinePaymentChoice(addressParams->type)) {
        case PAYMENT_PATH: {
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Payment key path"), MAX_BIP44_PATH_STRING_LENGTH, format_bip44_path, &addressParams->paymentKeyPath);
            break;
        }

        case PAYMENT_SCRIPT_HASH: {
            UI_ADD_FORMAT3(UI_STATIC_LABEL("Payment script hash"), MAX_BECH32_STRING_LENGTH, format_bech32, "script", addressParams->paymentScriptHash, SIZEOF(addressParams->paymentScriptHash));
            break;
        }

        default:
            // includes PAYMENT_NONE
            LEDGER_ASSERT(false, "Invalid payment choice");
            ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
    }
}

void addStakingInfoUIPair(const addressParams_t* addressParams) {
    switch (addressParams->stakingDataSource) {
        case NO_STAKING: {
            char* value_tmp = (char*) app_mem_alloc(MAX_BIP44_PATH_STRING_LENGTH + UI_BUFFER_SAFETY_MARGIN);
            if (value_tmp == NULL) {
                ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
            } else {
                switch (addressParams->type) {
                    case BYRON:
                        strncpy(value_tmp, "Byron address (no staking rewards)", MAX_BIP44_PATH_STRING_LENGTH + UI_BUFFER_SAFETY_MARGIN);
                        if (!ui_pairs_add_static_label(UI_STATIC_LABEL("WARNING:"), value_tmp)) {
                            ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
                        }
                        break;

                    case ENTERPRISE_KEY:
                    case ENTERPRISE_SCRIPT:
                        strncpy(value_tmp, "No staking rewards", MAX_BIP44_PATH_STRING_LENGTH + UI_BUFFER_SAFETY_MARGIN);
                        if (!ui_pairs_add_static_label(UI_STATIC_LABEL("WARNING:"), value_tmp)) {
                            ui_set_error_status(UI_STATUS_OUT_OF_MEMORY);
                        }
                        break;

                    default:
                        LEDGER_ASSERT(false, "Invalid address type for NO_STAKING");
                }
            }
            break;
        }

        case STAKING_KEY_PATH: {
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Staking path"), MAX_BIP44_PATH_STRING_LENGTH, format_bip44_path, &addressParams->stakingKeyPath);
            break;
        }

        case STAKING_KEY_HASH: {
            UI_ADD_FORMAT3(UI_STATIC_LABEL("Stake key hash"), MAX_BECH32_STRING_LENGTH, format_bech32, "stake_vkh", addressParams->stakingKeyHash, SIZEOF(addressParams->stakingKeyHash));
            break;
        }

        case STAKING_SCRIPT_HASH: {
            UI_ADD_FORMAT3(UI_STATIC_LABEL("Stake script hash"), MAX_BECH32_STRING_LENGTH, format_bech32, "script", addressParams->stakingScriptHash, SIZEOF(addressParams->stakingScriptHash));
            break;
        }

        case BLOCKCHAIN_POINTER: {
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Stake key pointer"), MAX_BIP44_PATH_STRING_LENGTH, format_blockchain_pointer, addressParams->stakingKeyBlockchainPointer);
            break;
        }

        default:
            LEDGER_ASSERT(false, "Invalid staking data source");
    }
}
