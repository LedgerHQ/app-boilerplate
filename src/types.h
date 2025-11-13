#pragma once

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t

// TODO somehow merge, maybe use standard functions instead of custom implementation in bip44
#include "bip32.h"
#include "addressUtils/bip44.h"

#include "constants.h"
#include "tx_types.h"
#include "opcert_types.h"
#include "keyDerivation.h"

/**
 * Enumeration with expected INS of APDU commands.
 */
typedef enum {
    INS_GET_VERSION = 0x03,     /// version of the application
    INS_GET_APP_NAME = 0x04,    /// name of the application
    INS_GET_PUBLIC_KEY = 0x10,  /// public key of corresponding BIP32 path
    INS_SIGN_TX = 0x21,          /// sign transaction with BIP32 path
    INS_SIGN_OPCERT = 0x22  /// sign operational certificate with BIP32 path
} command_e;

/**
 * Enumeration with parsing state.
 */
typedef enum {
    STATE_NONE,     /// No state
    STATE_PARSED,   /// Transaction data parsed
    STATE_APPROVED  /// Transaction data approved
} state_e;

/**
 * Enumeration with user request type.
 */
typedef enum {
    REQUEST_EXPORT_PUBKEY,     /// export public key
    REQUEST_CONFIRM_ADDRESS,     /// confirm address derived from public key
    REQUEST_CONFIRM_TRANSACTION, /// confirm transaction information
    REQUEST_SIGN_OPCERT,         /// sign operational certificate
} request_type_e;

/**
 * Structure for public key context information.
 */
typedef struct {
    bip44_path_t path;
    extendedPublicKey_t extPubKey;
    bool silentExport;
} pubkey_ctx_t;

/**
 * ED25519 signature length constant
 */
#define ED25519_SIGNATURE_LENGTH 64

/**
 * Structure for single account data - enforces single account security model.
 * Tracks the account number and Byron/Shelley prefix of the first witness
 * to ensure all subsequent witnesses use the same account.
 */
typedef struct {
    bool isStored;           /// whether account data has been stored
    bool isByron;            /// whether the stored path uses Byron prefix (true) or Shelley prefix (false)
    uint32_t accountNumber;  /// the account number extracted from BIP44 path
} single_account_data_t;

/**
 * Structure for transaction information context.
 */
typedef struct {
    uint8_t *raw_tx;                      /// raw transaction serialized (dynamically allocated)
    size_t raw_tx_len;                    /// length of raw transaction
    transaction_t transaction;            /// structured transaction
    uint8_t tx_hash[32];                   /// transaction hash (Blake2b-256)

    // Witness signing fields (used after STATE_APPROVED)
    uint16_t num_witnesses;               /// total number of witnesses expected
    uint16_t current_witness;             /// current witness being processed
    bip44_path_t witness_path;            /// current witness path
    uint8_t witness_signature[ED25519_SIGNATURE_LENGTH];  /// current witness signature

    // Single account security model - ensures all witnesses use same account
    single_account_data_t single_account_data;  /// tracks account for multi-witness transactions

    // Warning collection (flist head) - network warnings only for now
    void* warning_list;                   /// head of warning flist (tx_warning_list_item_t)
} transaction_ctx_t;

/**
 * Structure for sign operational certificate information context.
 */
#define MAX_OPCERT_LENGTH (KES_PUBLIC_KEY_LENGTH + 8 + 8 + BIP44_MAX_PATH_SIZE)

typedef struct {
    uint8_t raw_opcert[MAX_OPCERT_LENGTH];
    size_t raw_opcert_len;
    parsed_opcert_t opcert;
    uint8_t signature[ED25519_SIGNATURE_LENGTH];
} sign_opcert_ctx_t;

/**
 * Structure for global context.
 */
typedef struct {
    state_e state;  /// state of the context
    union {
        pubkey_ctx_t pk_info;       /// public key context
        transaction_ctx_t tx_info;  /// transaction context
        sign_opcert_ctx_t opcert_info;
    };
    request_type_e req_type;              /// user request
    uint32_t bip32_path[MAX_BIP32_PATH];  /// BIP32 path
    uint8_t bip32_path_len;               /// length of BIP32 path
} global_ctx_t;
