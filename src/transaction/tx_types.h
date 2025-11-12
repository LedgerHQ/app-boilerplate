#pragma once

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t
#include <stdbool.h> // bool
#include "utils/list.h"
#include "addressUtils/bip44.h"

#define MAX_TX_LEN   510
#define ADDRESS_LEN  20

#ifndef TX_HASH_LENGTH
#define TX_HASH_LENGTH 32
#endif

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
// Withdrawal credential structure (similar to address params but for staking only)
typedef struct {
    uint8_t type;  // staking_data_source_t: KEY_PATH, KEY_HASH, or SCRIPT_HASH
    union {
        bip44_path_t keyPath;
        uint8_t keyHash[28];    // ADDRESS_KEY_HASH_LENGTH from cardano.h
        uint8_t scriptHash[28]; // SCRIPT_HASH_LENGTH from cardano.h
    };
} withdrawal_credential_t;

// Withdrawal structure (reward withdrawal from staking account)
typedef struct {
    withdrawal_credential_t credential;
    uint64_t amount;
    uint8_t previousRewardAccount[29];  // REWARD_ACCOUNT_SIZE from cardano.h (1 + 28)
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
    bool includeTtl;        /// whether TTL is included
    bool includeValidityIntervalStart;  /// whether validity interval start is included

    // Transaction data (parsed from raw tx buffer)
    s_flist_node *inputs;   /// linked list of inputs (tx_input_list_item_t)
    s_flist_node *outputs;  /// linked list of outputs (tx_output_list_item_t)
    s_flist_node *withdrawals;  /// linked list of withdrawals (tx_withdrawal_list_item_t)
    uint64_t fee;           /// fee (8 bytes)
    uint64_t ttl;           /// time-to-live (optional, only if includeTtl is true)
    uint64_t validityIntervalStart;  /// validity interval start (optional, slot 8)
} transaction_t;
