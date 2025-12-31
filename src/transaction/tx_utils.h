#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "transaction/tx.h"
#include "addressUtils/bip44.h"

/**
 * Checks if a witness path violates the single-account security model.
 *
 * The single-account security model ensures that all witnesses in a transaction
 * use the same BIP44 account number. This prevents cross-account signing which
 * could indicate a confused deputy attack.
 *
 * On the first call, stores the account information from the path.
 * On subsequent calls, checks if the path uses the same account and Byron/Shelley prefix.
 * Byron and Shelley prefixes can be mixed only at account 0 (hardened(0)).
 *
 * @param[in] path
 *   BIP44 path to check and potentially store
 *
 * @return
 *   true if the path violates the single-account security model
 *   false if the path is acceptable or was stored successfully
 */
bool violatesSingleAccountOrStoreIt(const bip44_path_t* path);

typedef struct {
    uint32_t total_owners;
    uint32_t path_owners;
} pool_owner_counts_t;

pool_owner_counts_t count_pool_owner_nodes(const s_flist_node* owners);
