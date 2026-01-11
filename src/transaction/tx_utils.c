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

#include "tx_utils.h"
#include "globals.h"
#include "bip44.h"
#include "utils/assert.h"
#include "utils/utils.h"

bool violatesSingleAccountOrStoreIt(const bip44_path_t* path) {
    LEDGER_ASSERT(path != NULL, "NULL path in single-account check");
    TRACE("Considering path");
    BIP44_PRINTF(path);
    TRACE("");

    single_account_data_t* singleAccountData = &(G_context.tx_info.single_account_data);

    if (!bip44_hasOrdinaryWalletKeyPrefix(path) || !bip44_containsAccount(path)) {
        TRACE("Invalid path in single account check");
        ASSERT(false);
    }

    const bool isByron = bip44_hasByronPrefix(path);
    const uint32_t account = bip44_getAccount(path);

    if (singleAccountData->isStored) {
        const uint32_t storedAccount = singleAccountData->accountNumber;
        if (account != storedAccount) {
            TRACE("Account mismatch: current=%d, stored=%d", account, storedAccount);
            return true;
        }
        const bool combinesByronAndShelley = singleAccountData->isByron != isByron;
        const bool combinationAllowed = (storedAccount == bip44_harden(0));
        if (combinesByronAndShelley && !combinationAllowed) {
            TRACE("Byron/Shelley mixing not allowed for account %d", storedAccount);
            return true;
        }
    } else {
        singleAccountData->isStored = true;
        singleAccountData->isByron = isByron;
        singleAccountData->accountNumber = account;
        TRACE("Stored single account data: account=%d, isByron=%d", account, isByron);
    }

    return false;
}

pool_owner_counts_t count_pool_owner_nodes(const s_flist_node* owners) {
    pool_owner_counts_t counts = {0};
    const s_flist_node* node = owners;
    while (node != NULL) {
        const tx_certificate_node_t* owner_item = (const tx_certificate_node_t*) node;
        const ext_credential_t* owner_cred = &owner_item->certificate.stakeCredential;
        if (owner_cred->type == EXT_CREDENTIAL_KEY_PATH) {
            counts.path_owners++;
        }
        counts.total_owners++;
        node = node->next;
    }
    return counts;
}
