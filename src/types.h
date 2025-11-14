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
    INS_GET_SERIAL = 0x01,      /// serial number of the device
    INS_GET_VERSION = 0x03,     /// version of the application
    INS_GET_APP_NAME = 0x04,    /// name of the application
    INS_GET_PUBLIC_KEY = 0x10,  /// public key of corresponding BIP32 path
    INS_SIGN_TX = 0x21,         /// sign transaction with BIP32 path
    INS_SIGN_OPCERT = 0x22      /// sign operational certificate with BIP32 path
} command_e;

/**
 * Enumeration with transaction signing state.
 */
typedef enum {
    TX_STATE_NONE,      /// No transaction being processed (truly idle)
    TX_STATE_CHUNKS,    /// Actively receiving transaction data chunks
    TX_STATE_PARSED,    /// Transaction data parsed and ready for approval
    TX_STATE_APPROVED   /// User approved, waiting for witness signatures
} tx_state_e;

/**
 * Enumeration with operational certificate signing state.
 */
typedef enum {
    OPCERT_STATE_NONE,      /// No OpCert being processed
    OPCERT_STATE_PARSED,    /// OpCert parsed and ready for approval
    OPCERT_STATE_APPROVED   /// User approved, waiting for signature
} opcert_state_e;

/**
 * Enumeration with user request type.
 */
typedef enum {
    REQUEST_NONE = 0,              /// No operation in progress (idle state)
    REQUEST_EXPORT_PUBKEY,         /// export public key
    REQUEST_SIGN_TRANSACTION,      /// confirm transaction information
    REQUEST_SIGN_OPCERT,           /// sign operational certificate
} request_type_e;

/**
 * Structure for public key context information.
 */
typedef struct {
    bip44_path_t path;
    extendedPublicKey_t extPubKey;
    bool silentExport;
} pubkey_ctx_t;

// ==============================  CARDANO PROTOCOL TYPES  ==============================

/**
 * Key reference type for address ownership.
 * Indicates whether a key is device-owned or a third-party key hash.
 */
typedef enum {
    KEY_REFERENCE_PATH = 1,  // aka 'DEVICE_OWNED', the address belongs to this device
    KEY_REFERENCE_HASH = 2,  // aka 'THIRD_PARTY', the address is third party
} key_reference_type_t;

/**
 * Reward account structure for managing stake pool rewards and delegations.
 * Can reference either a key derivation path or a third-party key hash.
 */
typedef struct {
    key_reference_type_t keyReferenceType;
    union {
        bip44_path_t path;
        uint8_t hashBuffer[REWARD_ACCOUNT_SIZE];
    };
} reward_account_t;

/**
 * Transaction input structure.
 * References a previous transaction output by hash and index.
 */
typedef struct {
    uint8_t txHashBuffer[TX_HASH_LENGTH];
    uint32_t index;
} tx_input_t;

/**
 * Token group for native tokens in transaction outputs.
 * Groups tokens by their minting policy ID.
 */
typedef struct {
    uint8_t policyId[MINTING_POLICY_ID_SIZE];
} token_group_t;

/**
 * Output token amount structure for native assets.
 */
typedef struct {
    uint8_t assetNameBytes[ASSET_NAME_SIZE_MAX];
    size_t assetNameSize;
    uint64_t amount;
} output_token_amount_t;

// ==============================  CERTIFICATE TYPES  ==============================

/**
 * Certificate type enumeration.
 * Represents different stake pool and governance certificate types.
 */
typedef enum {
    CERTIFICATE_STAKE_REGISTRATION = 0,
    CERTIFICATE_STAKE_DEREGISTRATION = 1,
    CERTIFICATE_STAKE_DELEGATION = 2,
    CERTIFICATE_STAKE_POOL_REGISTRATION = 3,
    CERTIFICATE_STAKE_POOL_RETIREMENT = 4,
    CERTIFICATE_STAKE_REGISTRATION_CONWAY = 7,
    CERTIFICATE_STAKE_DEREGISTRATION_CONWAY = 8,
    CERTIFICATE_VOTE_DELEGATION = 9,
    CERTIFICATE_AUTHORIZE_COMMITTEE_HOT = 14,
    CERTIFICATE_RESIGN_COMMITTEE_COLD = 15,
    CERTIFICATE_DREP_REGISTRATION = 16,
    CERTIFICATE_DREP_DEREGISTRATION = 17,
    CERTIFICATE_DREP_UPDATE = 18,
} certificate_type_t;

// ==============================  RELAY TYPES  ==============================

/**
 * Relay format for pool information distribution.
 * Indicates how the pool relay address is specified.
 */
typedef enum {
    RELAY_SINGLE_HOST_IP = 0,
    RELAY_SINGLE_HOST_NAME = 1,
    RELAY_MULTIPLE_HOST_NAME = 2
} relay_format_t;

/**
 * IPv4 address structure for pool relays.
 */
typedef struct {
    bool isNull;
    uint8_t ip[IPV4_SIZE];
} ipv4_t;

/**
 * IPv6 address structure for pool relays.
 */
typedef struct {
    bool isNull;
    uint8_t ip[IPV6_SIZE];
} ipv6_t;

/**
 * IP port structure for pool relay configuration.
 */
typedef struct {
    bool isNull;
    uint16_t number;
} ipport_t;

/**
 * Pool relay structure combining address and port information.
 */
typedef struct {
    relay_format_t format;

    ipport_t port;

    ipv4_t ipv4;
    ipv6_t ipv6;

    size_t dnsNameSize;
    uint8_t dnsName[DNS_NAME_SIZE_MAX];
} pool_relay_t;

// ==============================  NATIVE SCRIPTS  ==============================

/**
 * Native script type enumeration.
 * Represents different types of native scripts that can be used in transactions.
 */
typedef enum {
    NATIVE_SCRIPT_PUBKEY = 0,
    NATIVE_SCRIPT_ALL = 1,
    NATIVE_SCRIPT_ANY = 2,
    NATIVE_SCRIPT_N_OF_K = 3,
    NATIVE_SCRIPT_INVALID_BEFORE = 4,
    NATIVE_SCRIPT_INVALID_HEREAFTER = 5,
} native_script_type_t;

// ==============================  CIP8 MESSAGE SIGNING  ==============================

/**
 * CIP8 message address field type.
 * Specifies whether the address field contains a full address or key hash.
 */
typedef enum {
    CIP8_ADDRESS_FIELD_ADDRESS = 1,
    CIP8_ADDRESS_FIELD_KEYHASH = 2,
} cip8_address_field_type_t;

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
    uint8_t tx_hash[TX_HASH_LENGTH];      /// transaction hash (Blake2b-256)

    // Witness signing fields (used after TX_STATE_APPROVED)
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
#define MAX_OPCERT_LENGTH (KES_PUBLIC_KEY_LENGTH + OPCERT_KES_PERIOD_SIZE + OPCERT_ISSUE_COUNTER_SIZE + BIP44_MAX_PATH_SIZE)

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
    /// Instruction-specific state (only one operation active at a time)
    union {
        tx_state_e tx_state;           /// Transaction signing state
        opcert_state_e opcert_state;   /// OpCert signing state
    } state;

    union {
        pubkey_ctx_t pk_info;       /// public key context
        transaction_ctx_t tx_info;  /// transaction context
        sign_opcert_ctx_t opcert_info;
    };
    request_type_e req_type;              /// user request
    uint32_t bip32_path[MAX_BIP32_PATH];  /// BIP32 path
    uint8_t bip32_path_len;               /// length of BIP32 path
} global_ctx_t;
