#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "os.h"
#include "ux.h"
#include "cardano_constants.h"
#include "bip32.h"
#include "securityPolicy/securityWarnings.h"
#include "transaction/tx.h"
#include "transaction/tx_state.h"
#include "opcert/opcert_types.h"
#include "apdu/apdu_constants.h"
#include "keyDerivation.h"

/**
 * Tracks stored account metadata for the single-account security model.
 */
typedef struct {
    bool isStored;
    bool isByron;
    uint32_t accountNumber;
} single_account_data_t;

/**
 * Transaction context (covers raw tx buffer + witness bookkeeping).
 */
typedef struct {
    uint8_t *raw_tx;
    size_t raw_tx_len;
    transaction_t transaction;
    uint8_t tx_hash[TX_HASH_LENGTH];

    uint16_t num_witnesses;
    uint16_t current_witness;
    bip44_path_t witness_path;
    uint8_t witness_signature[ED25519_SIGNATURE_LENGTH];

    bool pool_owner_path_present;
    bip44_path_t pool_owner_path;

    single_account_data_t single_account_data;

    warning_bits_t warning_bits;
    uint16_t planned_ui_pairs;
} transaction_ctx_t;

/**
 * Operational certificate context.
 */
#define MAX_OPCERT_LENGTH (KES_PUBLIC_KEY_LENGTH + OPCERT_KES_PERIOD_SIZE + OPCERT_ISSUE_COUNTER_SIZE + BIP44_MAX_PATH_SIZE)

typedef struct {
    uint8_t raw_opcert[MAX_OPCERT_LENGTH];
    size_t raw_opcert_len;
    parsed_opcert_t opcert;
    uint8_t signature[ED25519_SIGNATURE_LENGTH];
} sign_opcert_ctx_t;

/**
 * Exposed context for public-key exports.
 */
typedef struct {
    bip44_path_t path;
    extendedPublicKey_t extPubKey;
    bool silentExport;
} pubkey_ctx_t;

/**
 * Global context for user requests.
 */
typedef struct {
    union {
        tx_state_e tx_state;
        opcert_state_e opcert_state;
    } state;

    union {
        pubkey_ctx_t pk_info;
        transaction_ctx_t tx_info;
        sign_opcert_ctx_t opcert_info;
    };

    request_type_e req_type;
    uint32_t bip32_path[MAX_BIP32_PATH];
    uint8_t bip32_path_len;
} global_ctx_t;

extern global_ctx_t G_context;

/**
 * Global structure for NVM data storage.
 */
typedef struct internal_storage_t {
    uint8_t expert_mode_enabled;
    uint8_t silent_pubkey_export_enabled;
    uint8_t initialized;
} internal_storage_t;

extern const internal_storage_t N_storage_real;
#define N_storage (*(volatile internal_storage_t *) PIC(&N_storage_real))
