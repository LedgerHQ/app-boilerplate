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
#include "securityPolicy/securityPolicy.h"
#include "securityPolicy/securityWarnings.h"
#include "transaction/tx_utils.h"
#include "utils/assert.h"
#include "utils/ipUtils.h"
#include "globals.h"
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
    LEDGER_ASSERT(keyPathLabel[0] != '\0', "Empty keyPathLabel");
    LEDGER_ASSERT(keyHashLabel[0] != '\0', "Empty keyHashLabel");
    LEDGER_ASSERT(keyHashPrefix[0] != '\0', "Empty keyHashPrefix");
    LEDGER_ASSERT(scriptHashLabel[0] != '\0', "Empty scriptHashLabel");
    LEDGER_ASSERT(scriptHashPrefix[0] != '\0', "Empty scriptHashPrefix");

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

    switch (voter->type) {
        case EXT_VOTER_COMMITTEE_HOT_KEY_PATH:
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Committee hot key"),
                           MAX_BIP44_PATH_STRING_LENGTH,
                           format_bip44_path,
                           &voter->keyPath);
            break;
        case EXT_VOTER_COMMITTEE_HOT_KEY_HASH:
            LEDGER_ASSERT(voter->keyHash != NULL, "NULL committee hot key hash voter");
            UI_ADD_FORMAT3(UI_STATIC_LABEL("Committee hot key hash"),
                           MAX_BECH32_STRING_LENGTH,
                           format_bech32,
                           "cc_hot",
                           voter->keyHash,
                           ADDRESS_KEY_HASH_LENGTH);
            break;
        case EXT_VOTER_COMMITTEE_HOT_SCRIPT_HASH:
            LEDGER_ASSERT(voter->scriptHash != NULL, "NULL committee hot script hash voter");
            UI_ADD_FORMAT3(UI_STATIC_LABEL("Committee hot script hash"),
                           MAX_BECH32_STRING_LENGTH,
                           format_bech32,
                           "cc_hot",
                           voter->scriptHash,
                           SCRIPT_HASH_LENGTH);
            break;
        case EXT_VOTER_DREP_KEY_PATH:
            UI_ADD_FORMAT1(UI_STATIC_LABEL("DRep key"),
                           MAX_BIP44_PATH_STRING_LENGTH,
                           format_bip44_path,
                           &voter->keyPath);
            break;
        case EXT_VOTER_DREP_KEY_HASH:
            LEDGER_ASSERT(voter->keyHash != NULL, "NULL drep key hash voter");
            UI_ADD_FORMAT3(UI_STATIC_LABEL("DRep key hash"),
                           MAX_BECH32_STRING_LENGTH,
                           format_bech32,
                           "drep",
                           voter->keyHash,
                           ADDRESS_KEY_HASH_LENGTH);
            break;
        case EXT_VOTER_DREP_SCRIPT_HASH:
            LEDGER_ASSERT(voter->scriptHash != NULL, "NULL drep script hash voter");
            UI_ADD_FORMAT3(UI_STATIC_LABEL("DRep script hash"),
                           MAX_BECH32_STRING_LENGTH,
                           format_bech32,
                           "drep",
                           voter->scriptHash,
                           SCRIPT_HASH_LENGTH);
            break;
        case EXT_VOTER_STAKE_POOL_KEY_PATH:
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Stake pool key"),
                           MAX_BIP44_PATH_STRING_LENGTH,
                           format_bip44_path,
                           &voter->keyPath);
            break;
        case EXT_VOTER_STAKE_POOL_KEY_HASH:
            LEDGER_ASSERT(voter->keyHash != NULL, "NULL stake pool key hash voter");
            UI_ADD_FORMAT3(UI_STATIC_LABEL("Stake pool key hash"),
                           MAX_BECH32_STRING_LENGTH,
                           format_bech32,
                           "pool",
                           voter->keyHash,
                           ADDRESS_KEY_HASH_LENGTH);
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
            LEDGER_ASSERT(drep->keyHash != NULL, "NULL drep key hash pointer");
            UI_ADD_FORMAT3(label, MAX_BECH32_STRING_LENGTH, format_bech32, "drep", drep->keyHash, ADDRESS_KEY_HASH_LENGTH);
            break;
        }
        case EXT_DREP_SCRIPT_HASH: {
            LEDGER_ASSERT(drep->scriptHash != NULL, "NULL drep script hash pointer");
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

static void addStakeCredentialUIPairs(const ext_credential_t* credential) {
    addCredentialUIPairs(
        credential,
        UI_STATIC_LABEL("Stake key"),
        UI_STATIC_LABEL("Stake key hash"),
        "stake_vkh",
        UI_STATIC_LABEL("Stake script hash"),
        "script"
    );
}

static void addDRepCredentialUIPairs(const ext_credential_t* credential) {
    addCredentialUIPairs(
        credential,
        UI_STATIC_LABEL("DRep key"),
        UI_STATIC_LABEL("DRep key hash"),
        "drep",
        UI_STATIC_LABEL("DRep script hash"),
        "drep"
    );
}

static void addCommitteeColdCredentialUIPairs(const ext_credential_t* credential) {
    addCredentialUIPairs(
        credential,
        UI_STATIC_LABEL("Committee cold key"),
        UI_STATIC_LABEL("Committee cold key hash"),
        "cc_cold",
        UI_STATIC_LABEL("Committee cold script hash"),
        "cc_cold"
    );
}

static void addCommitteeHotCredentialUIPairs(const ext_credential_t* credential) {
    addCredentialUIPairs(
        credential,
        UI_STATIC_LABEL("Committee hot key"),
        UI_STATIC_LABEL("Committee hot key hash"),
        "cc_hot",
        UI_STATIC_LABEL("Committee hot script hash"),
        "cc_hot_script"
    );
}

static void addVoterCredentialUIPairs(const ext_credential_t* credential) {
    addCredentialUIPairs(
        credential,
        UI_STATIC_LABEL("Voter"),
        UI_STATIC_LABEL("Voter hash"),
        "stake_vkh",
        UI_STATIC_LABEL("Voter script hash"),
        "script"
    );
}

static void addPoolRetirementUIPairs(const certificate_data_t* certificate_data) {
    const ext_credential_t* pool_credential = &certificate_data->poolCredential;
    uint8_t pool_key_hash[POOL_KEY_HASH_LENGTH];

    switch (pool_credential->type) {
        case EXT_CREDENTIAL_KEY_PATH:
            bip44_pathToKeyHash(&pool_credential->keyPath, pool_key_hash, sizeof(pool_key_hash));
            break;
        case EXT_CREDENTIAL_KEY_HASH: {
            STATIC_ASSERT(ADDRESS_KEY_HASH_LENGTH == POOL_KEY_HASH_LENGTH,
                          "pool credential hash size mismatch");
            memcpy(pool_key_hash, pool_credential->keyHash, POOL_KEY_HASH_LENGTH);
            break;
        }
        default:
            LEDGER_ASSERT(false, "Unsupported pool credential type for retirement");
    }

    UI_ADD_FORMAT3(UI_STATIC_LABEL("Pool ID"),
                   MAX_BECH32_STRING_LENGTH,
                   format_bech32,
                   "pool",
                   pool_key_hash,
                   POOL_KEY_HASH_LENGTH);

    UI_ADD_FORMAT1(UI_STATIC_LABEL("Retirement epoch"),
                   MAX_UINT64_STRING_LENGTH,
                   format_uint64,
                   certificate_data->retirementEpoch);
}

void addAnchorUIPairs(const anchor_t *anchor) {
    LEDGER_ASSERT(anchor != NULL, "NULL anchor");

    if (!anchor->isIncluded) {
        return;
    }

    UI_ADD_FORMAT2(UI_STATIC_LABEL("Anchor URL"), MAX_ANCHOR_URL_LENGTH, format_url, anchor->url, anchor->urlLength);
    UI_ADD_FORMAT3(UI_STATIC_LABEL("Anchor hash"), MAX_BECH32_STRING_LENGTH, format_bech32, "anchor", anchor->hash, ANCHOR_HASH_LENGTH);
}

void addWithdrawalUIPairs(uint8_t networkId, const withdrawal_t *withdrawal) {
    LEDGER_ASSERT(withdrawal != NULL, "NULL withdrawal");

    UI_ADD_FORMAT1(UI_STATIC_LABEL("Withdrawal"), MAX_ADA_AMOUNT_STRING_LENGTH, format_ada_amount, withdrawal->amount);

    const ext_credential_t *credential = &withdrawal->stakeCredential;

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

void addCertificateUIPairs(const certificate_data_t* certificate_data) {
    LEDGER_ASSERT(certificate_data != NULL, "NULL certificate data");

    TRACE("Formatting certificate type=%u", certificate_data->type);
    UI_ADD_FORMAT1(UI_STATIC_LABEL("Certificate"),
                   MAX_CERTIFICATE_TYPE_LENGTH,
                   format_certificate_type,
                   certificate_data->type);

    switch (certificate_data->type) {
        case CERTIFICATE_STAKE_REGISTRATION:
        case CERTIFICATE_STAKE_DEREGISTRATION: {
            addStakeCredentialUIPairs(&certificate_data->stakeCredential);
            break;
        }

        case CERTIFICATE_STAKE_DELEGATION: {
            addStakeCredentialUIPairs(&certificate_data->stakeCredential);
            UI_ADD_FORMAT3(UI_STATIC_LABEL("Pool"),
                           MAX_BECH32_STRING_LENGTH,
                           format_bech32,
                           "pool",
                           certificate_data->poolKeyHash,
                           POOL_KEY_HASH_LENGTH);
            break;
        }

        case CERTIFICATE_STAKE_REGISTRATION_CONWAY:
        case CERTIFICATE_STAKE_DEREGISTRATION_CONWAY: {
            addStakeCredentialUIPairs(&certificate_data->stakeCredential);
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Deposit"),
                           MAX_ADA_AMOUNT_STRING_LENGTH,
                           format_ada_amount,
                           certificate_data->deposit);
            break;
        }

        case CERTIFICATE_STAKE_POOL_RETIREMENT: {
            addPoolRetirementUIPairs(certificate_data);
            break;
        }

        case CERTIFICATE_VOTE_DELEGATION: {
            addVoterCredentialUIPairs(&certificate_data->stakeCredential);
            addDRepUIPairs(&certificate_data->drep, UI_STATIC_LABEL("DRep"));
            break;
        }

        case CERTIFICATE_AUTHORIZE_COMMITTEE_HOT: {
            addCommitteeColdCredentialUIPairs(&certificate_data->coldCredential);
            addCommitteeHotCredentialUIPairs(&certificate_data->hotCredential);
            break;
        }

        case CERTIFICATE_RESIGN_COMMITTEE_COLD: {
            addCommitteeColdCredentialUIPairs(&certificate_data->coldCredential);
            addAnchorUIPairs(&certificate_data->anchor);
            break;
        }

        case CERTIFICATE_DREP_REGISTRATION: {
            addDRepCredentialUIPairs(&certificate_data->dRepCredential);
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Deposit"),
                           MAX_ADA_AMOUNT_STRING_LENGTH,
                           format_ada_amount,
                           certificate_data->deposit);
            addAnchorUIPairs(&certificate_data->anchor);
            break;
        }

        case CERTIFICATE_DREP_DEREGISTRATION: {
            addDRepCredentialUIPairs(&certificate_data->dRepCredential);
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Deposit"),
                           MAX_ADA_AMOUNT_STRING_LENGTH,
                           format_ada_amount,
                           certificate_data->deposit);
            break;
        }

        case CERTIFICATE_DREP_UPDATE: {
            addDRepCredentialUIPairs(&certificate_data->dRepCredential);
            addAnchorUIPairs(&certificate_data->anchor);
            break;
        }

        case CERTIFICATE_STAKE_POOL_REGISTRATION:
            LEDGER_ASSERT(false, "CERTIFICATE_STAKE_POOL_REGISTRATION should be treated separately");
            break;

        default:
            LEDGER_ASSERT(false, "Unknown certificate type");
    }
}

void addPaymentInfoUIPairs(const addressParams_t* addressParams) {
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
    }
}

void addStakingInfoUIPairs(const addressParams_t* addressParams) {
    switch (addressParams->stakingDataSource) {
        case NO_STAKING: {
            UI_ADD_FORMAT1(UI_STATIC_LABEL("Warning:"), MAX_BIP44_PATH_STRING_LENGTH, format_constant_string, "no staking rewards");
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
