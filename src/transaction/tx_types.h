#pragma once

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t
#include "utils/list.h"

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
    bool includeTtl;        /// whether TTL is included

    // Transaction data (parsed from raw tx buffer)
    s_flist_node *inputs;   /// linked list of inputs (tx_input_list_item_t)
    s_flist_node *outputs;  /// linked list of outputs (tx_output_list_item_t)
    uint64_t fee;           /// fee (8 bytes)
    uint64_t ttl;           /// time-to-live (optional, only if includeTtl is true)
} transaction_t;
