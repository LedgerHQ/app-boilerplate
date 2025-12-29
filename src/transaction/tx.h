#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cardano_constants.h"
#include "memory/flist.h"
#include "addressUtils/bip44.h"
#include "transaction/tx_aux_data_types.h"
#include "transaction/tx_credential_types.h"
#include "transaction/tx_certificate_types.h"
#include "transaction/tx_output_types.h"

// Mint token limits
// Note: No artificial limits on asset groups or tokens per mint.
// The wire format uses uint16_t for counts, so the natural limit is UINT16_MAX.
// Memory allocation is dynamic, so we can handle any count up to that limit.
#define MAX_MINT_ASSET_NAME_LENGTH 32

typedef struct {
    const uint8_t* txHash;
    uint32_t index;
} tx_input_t;

typedef struct {
    const uint8_t* assetName;
    uint8_t assetNameLen;
    int64_t amount;
} mint_token_t;

typedef struct {
    const uint8_t* policyId;
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
} withdrawal_data_t;

typedef enum {
    REQUIRED_SIGNER_WITH_PATH = 0,
    REQUIRED_SIGNER_WITH_HASH = 1,
} required_signer_type_t;

typedef struct {
    required_signer_type_t type;
    union {
        bip44_path_t keyPath;
        uint8_t keyHash[ADDRESS_KEY_HASH_LENGTH];
    };
} required_signer_t;

typedef struct {
    s_flist_node node;
    withdrawal_data_t withdrawal_data;
} tx_withdrawal_list_item_t;

typedef struct {
    s_flist_node node;
    required_signer_t required_signer_data;
} tx_required_signer_list_item_t;

// Collateral inputs use the same structure as regular inputs
typedef tx_input_list_item_t tx_collateral_input_list_item_t;

// Certificate data structure supporting multiple certificate types
// Fields are used selectively depending on certificate type:
// - STAKE_REGISTRATION/DEREGISTRATION: stakeCredential
// - STAKE_REGISTRATION_CONWAY/DEREGISTRATION_CONWAY: stakeCredential, deposit
// - STAKE_DELEGATION: stakeCredential, poolKeyHash
// - STAKE_POOL_RETIREMENT: poolCredential, retirementEpoch
// - VOTE_DELEGATION: stakeCredential, drep
// - AUTHORIZE_COMMITTEE_HOT: coldCredential, hotCredential
// - RESIGN_COMMITTEE_COLD: coldCredential, anchor
// - DREP_REGISTRATION/UPDATE: dRepCredential, deposit (reg only), anchor
// - DREP_DEREGISTRATION: dRepCredential, deposit
typedef struct {
    certificate_type_t type;
    union {
        ext_credential_t stakeCredential;
        ext_credential_t coldCredential;
        ext_credential_t dRepCredential;
        ext_credential_t poolCredential;
    };
    union {
        ext_credential_t hotCredential;
        const uint8_t* poolKeyHash;
        uint64_t deposit;
        uint64_t retirementEpoch;
        ext_drep_t drep;
    };
    anchor_t anchor;  // For committee resign, DRep registration/update
} certificate_data_t;

typedef struct {
    s_flist_node node;
    certificate_data_t certificate_data;
} tx_certificate_list_item_t;

typedef struct {
    // signing / network metadata
    sign_tx_signingmode_t txSigningMode;
    uint8_t networkId;
    uint32_t protocolMagic;
    bool tagCborSets;

    // Note: We use linked lists (via flist) for parsed transaction items because
    // the memory allocated to list nodes may be gradually reused/reallocated for UI
    // string formatting during processing. Arrays would prevent this reallocation.

    // CBOR key order (matches transaction_body CDDL)
    uint16_t num_inputs;                // key 0
    s_flist_node* inputs;

    uint16_t num_outputs;               // key 1
    s_flist_node* outputs;

    uint64_t fee;                       // key 2

    bool includeTtl;                    // key 3
    uint64_t ttl;

    uint16_t num_certificates;          // key 4
    s_flist_node* certificates;

    uint16_t num_withdrawals;           // key 5
    s_flist_node* withdrawals;

    bool includeValidityIntervalStart;  // key 8
    uint64_t validityIntervalStart;

    uint16_t num_mint_asset_groups;     // key 9 (mint)
    s_flist_node* mint_asset_groups;
    bool includeAuxDataHash;
    aux_data_type_t auxDataType;
    uint8_t auxDataHash[AUX_DATA_HASH_LENGTH];

    bool includeScriptDataHash;         // key 11
    uint8_t scriptDataHash[SCRIPT_DATA_HASH_LENGTH];

    uint16_t num_collateral_inputs;     // key 13
    s_flist_node* collateral_inputs;

    uint16_t num_required_signers;      // key 14
    s_flist_node* required_signers;

    bool includeNetworkId;              // key 15

    bool includeCollateralOutput;       // key 16
    struct {
        tx_output_destination_storage_t destination;
        uint64_t adaAmount;
        uint16_t numAssetGroups;
        asset_group_t* assetGroups;
        output_datum_t datum;
        bool hasRefScript;
        ref_script_t refScript;
        tx_output_serialization_format_t format;
    } collateral_output;

    bool includeTotalCollateral;        // key 17
    uint64_t totalCollateral;

    uint16_t num_reference_inputs;      // key 18
    s_flist_node* reference_inputs;

    bool includeTreasury;                // key 21
    uint64_t treasury;

    bool includeDonation;                // key 22
    uint64_t donation;
} transaction_t;
