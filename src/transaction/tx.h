#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cardano_constants.h"
#include "memory/flist.h"
#include "addressUtils/bip44.h"
#include "transaction/tx_credential_types.h"
#include "transaction/tx_certificate_types.h"
#include "transaction/tx_output_types.h"

// Mint token limits
#define MAX_MINT_ASSET_GROUPS 100
#define MAX_TOKENS_PER_MINT_GROUP 100
#define MAX_MINT_ASSET_NAME_LEN 32

typedef struct {
    uint8_t txHashBuffer[TX_HASH_LENGTH];
    uint32_t index;
} tx_input_t;

typedef struct {
    uint8_t assetName[MAX_MINT_ASSET_NAME_LEN];
    uint8_t assetNameLen;
    int64_t amount;
} mint_token_t;

typedef struct {
    uint8_t policyId[MINTING_POLICY_ID_SIZE];
    uint16_t numTokens;
    mint_token_t* tokens;
} mint_asset_group_t;

typedef struct {
    s_flist_node node;
    mint_asset_group_t asset_group;
} mint_asset_group_list_item_t;

typedef enum {
    SIGN_TX_SIGNINGMODE_ORDINARY_TX = 3,
    SIGN_TX_SIGNINGMODE_POOL_REGISTRATION_OWNER = 4,
    SIGN_TX_SIGNINGMODE_POOL_REGISTRATION_OPERATOR = 5,
    SIGN_TX_SIGNINGMODE_MULTISIG_TX = 6,
    SIGN_TX_SIGNINGMODE_PLUTUS_TX = 7,
} sign_tx_signingmode_t;

typedef enum {
    TX_OPTIONS_TAG_CBOR_SETS = 1,  // Whether to tag CBOR sets in transaction hash
} tx_options_e;

typedef struct {
    s_flist_node node;
    tx_input_t input_data;
} tx_input_list_item_t;

typedef struct {
    ext_credential_t stakeCredential;
    uint64_t amount;
    uint8_t previousRewardAccount[REWARD_ACCOUNT_SIZE];
} withdrawal_data_t;

typedef struct {
    s_flist_node node;
    withdrawal_data_t withdrawal_data;
} tx_withdrawal_list_item_t;

typedef struct {
    // signing / network metadata
    sign_tx_signingmode_t txSigningMode;
    uint8_t networkId;
    uint32_t protocolMagic;
    bool tagCborSets;

    // CBOR key order (matches transaction_body CDDL)
    uint16_t num_inputs;                // key 0
    s_flist_node* inputs;

    uint16_t num_outputs;               // key 1
    s_flist_node* outputs;

    uint64_t fee;                       // key 2

    bool includeTtl;                    // key 3
    uint64_t ttl;

    // certificates (key 4) handled elsewhere

    uint16_t num_withdrawals;           // key 5
    s_flist_node* withdrawals;

    bool includeValidityIntervalStart;  // key 8
    uint64_t validityIntervalStart;

    uint16_t num_mint_asset_groups;     // key 9 (mint)
    s_flist_node* mint_asset_groups;
} transaction_t;
