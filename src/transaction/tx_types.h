#pragma once

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t
#include <stdbool.h> // bool

#include "constants.h"
#include "memory/flist.h"
#include "addressUtils/bip44.h"
#include "handler/sign_tx.h"


// Hash and account constants (from cardano.h to avoid circular includes)
#define ADDRESS_KEY_HASH_LENGTH 28
#define SCRIPT_HASH_LENGTH 28
#define REWARD_ACCOUNT_SIZE (1 + ADDRESS_KEY_HASH_LENGTH)

// Mint token limits
#define MAX_MINT_ASSET_GROUPS 100
#define MAX_TOKENS_PER_MINT_GROUP 100
#define MAX_MINT_ASSET_NAME_LEN 32

// Mint token structure (within an asset group)
typedef struct {
    uint8_t assetName[MAX_MINT_ASSET_NAME_LEN];  // Asset name (variable length)
    uint8_t assetNameLen;                         // Length of asset name (0-32)
    int64_t amount;                               // Amount (signed - can be negative for burning)
} mint_token_t;

// Mint asset group (policy ID + tokens)
typedef struct {
    uint8_t policyId[MINTING_POLICY_ID_SIZE];    // Policy ID (no length prefix)
    uint16_t numTokens;                           // Number of tokens in this group
    mint_token_t* tokens;                         // Dynamically allocated array of tokens
} mint_asset_group_t;

// Mint asset group list item with flist node
typedef struct {
    s_flist_node node;              /// flist node for linked list
    mint_asset_group_t asset_group;
} mint_asset_group_list_item_t;

// Extended credential type (allows key path, key hash, or script hash)
typedef enum {
    // enum values are affected by backwards-compatibility
    EXT_CREDENTIAL_KEY_PATH = 0,
    EXT_CREDENTIAL_KEY_HASH = 2,
    EXT_CREDENTIAL_SCRIPT_HASH = 1,
} ext_credential_type_t;

// Extended credential structure
typedef struct {
    ext_credential_type_t type;
    union {
        bip44_path_t keyPath;
        uint8_t keyHash[ADDRESS_KEY_HASH_LENGTH];
        uint8_t scriptHash[SCRIPT_HASH_LENGTH];
    };
} ext_credential_t;

// Extended DREP type (allows key hash, key path, or script hash)
typedef enum {
    EXT_DREP_KEY_HASH = 0,
    EXT_DREP_KEY_PATH = 0 + 100,
    EXT_DREP_SCRIPT_HASH = 1,
    EXT_DREP_ABSTAIN = 2,
    EXT_DREP_NO_CONFIDENCE = 3,
} ext_drep_type_t;

// Extended DREP structure
typedef struct {
    ext_drep_type_t type;
    union {
        uint8_t keyHash[ADDRESS_KEY_HASH_LENGTH];
        bip44_path_t keyPath;
        uint8_t scriptHash[SCRIPT_HASH_LENGTH];
    };
} ext_drep_t;

// Transaction signing mode (affects restrictions on tx being signed)
typedef enum {
    SIGN_TX_SIGNINGMODE_ORDINARY_TX = 3,  // enum value 3 is needed for backwards compatibility
    SIGN_TX_SIGNINGMODE_POOL_REGISTRATION_OWNER = 4,
    SIGN_TX_SIGNINGMODE_POOL_REGISTRATION_OPERATOR = 5,
    SIGN_TX_SIGNINGMODE_MULTISIG_TX = 6,
    SIGN_TX_SIGNINGMODE_PLUTUS_TX = 7,
} sign_tx_signingmode_t;

// Input list item with flist node
// Contains tx hash and index matching the layout of tx_input_t from cardano.h
typedef struct {
    s_flist_node node;      /// flist node for linked list
    struct {
        uint8_t txHashBuffer[TX_HASH_LENGTH];
        uint32_t index;
    } input_data;
} tx_input_list_item_t;

// Withdrawal types (credential for reward withdrawals)
// Withdrawal structure (reward withdrawal from staking account)
typedef struct {
    ext_credential_t stakeCredential;
    uint64_t amount;
    uint8_t previousRewardAccount[REWARD_ACCOUNT_SIZE];
} withdrawal_data_t;

// Withdrawal list item with flist node
typedef struct {
    s_flist_node node;      /// flist node for linked list
    withdrawal_data_t withdrawal_data;
} tx_withdrawal_list_item_t;

// Output types are defined in tx_output_types.h (includes txHashBuilder dependencies)

typedef struct {
    // Transaction metadata (parsed from INIT APDU, before raw tx data)
    sign_tx_signingmode_t txSigningMode;  /// signing mode
    uint8_t networkId;                     /// network ID (0 = testnet, 1 = mainnet)
    uint32_t protocolMagic;                /// protocol magic number
    bool tagCborSets;                      /// whether to tag CBOR sets in tx hash

    // Transaction structure counts (from INIT APDU)
    uint16_t num_inputs;    /// number of inputs
    uint16_t num_outputs;   /// number of outputs
    uint16_t num_withdrawals;  /// number of withdrawals
    uint16_t num_mint_asset_groups;  /// number of mint asset groups
    bool includeTtl;        /// whether TTL is included
    bool includeValidityIntervalStart;  /// whether validity interval start is included

    // Transaction data (parsed from raw tx buffer)
    s_flist_node *inputs;   /// linked list of inputs (tx_input_list_item_t)
    s_flist_node *outputs;  /// linked list of outputs (tx_output_list_item_t)
    s_flist_node *withdrawals;  /// linked list of withdrawals (tx_withdrawal_list_item_t)
    s_flist_node *mint_asset_groups;  /// linked list of mint asset groups (mint_asset_group_list_item_t)
    uint64_t fee;           /// fee (8 bytes)
    uint64_t ttl;           /// time-to-live (optional, only if includeTtl is true)
    uint64_t validityIntervalStart;  /// validity interval start (optional, slot 8)
} transaction_t;
