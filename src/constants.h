#pragma once

/**
 * Instruction class of the Boilerplate application.
 */
#define CLA 0xD7

/**
 * Length of APPNAME variable in the Makefile.
 */
#define APPNAME_LEN (sizeof(APPNAME) - 1)

/**
 * Maximum length of MAJOR_VERSION || MINOR_VERSION || PATCH_VERSION.
 */
#define APPVERSION_LEN 3

/**
 * Maximum length of application name.
 */
#define MAX_APPNAME_LEN 64

#define MAX_UINT64_STRING_SIZE 21

/**
 * Buffer sizes for UI display strings.
 */
#define MAX_ADA_AMOUNT_STRING_SIZE 30      // For formatted ADA amounts (e.g., "123.456789 ADA")
#define MAX_WARNING_MESSAGE_SIZE 128       // For warning/error message text
#define MAX_OUTPUT_LABEL_SIZE 32           // For output labels (e.g., "Output 999 Address"); supports up to 999 outputs
#define MAX_AMOUNT_DISPLAY_SIZE 40         // For formatted amount display with currency (e.g., "BOL 123.456789")
#define MAX_TX_HASH_DISPLAY_SIZE 65        // For transaction hash hex display (32 bytes = 64 hex chars + null terminator)

/**
 * Item inclusion flags (for optional transaction fields).
 * Used to indicate whether an optional field is present in transaction data.
 */
typedef enum {
    ITEM_INCLUDED_NO = 1,   // Field is not included
    ITEM_INCLUDED_YES = 2,  // Field is included
} item_included_e;

/**
 * Transaction option flags for transaction hashing and processing.
 */
typedef enum {
    TX_OPTIONS_TAG_CBOR_SETS = 1,  // Whether to tag CBOR sets in transaction hash
} tx_options_e;

/**
 * High fee warning threshold (in lovelace).
 * If a transaction fee exceeds this value, a warning is shown to the user.
 */
#define HIGH_FEE_WARNING_THRESHOLD 5000000

// ==============================  CARDANO BLOCKCHAIN CONSTANTS  ==============================

/**
 * Lovelace (ADA in smallest units) constants.
 * 1 ADA = 1,000,000 lovelace
 */
#define LOVELACE_MAX_SUPPLY 45000000000000000  // 45 billion ADA * 10^6
#define LOVELACE_INVALID    47000000000000000  // Invalid sentinel value

/**
 * Cryptographic hash and key lengths (in bytes).
 */
#define ED25519_SIGNATURE_LENGTH               64
#define ED25519_EXTENDED_PRIVKEY_LENGTH        64  // Extended Ed25519 private key (32 bytes seed + 32 bytes)
#define ED25519_PUBKEY_UNCOMPRESSED_LENGTH     65  // Uncompressed Ed25519 public key (1 byte prefix + 32 bytes X + 32 bytes Y)
#define ADDRESS_KEY_HASH_LENGTH                28
#define POOL_KEY_HASH_LENGTH                   28
#define VRF_KEY_HASH_LENGTH                    32
#define TX_HASH_LENGTH                         32
#define AUX_DATA_HASH_LENGTH                   32
#define POOL_METADATA_HASH_LENGTH              32
#define CVOTE_REGISTRATION_PAYLOAD_HASH_LENGTH 32
#define SCRIPT_HASH_LENGTH                     28
#define SCRIPT_DATA_HASH_LENGTH                32
#define OUTPUT_DATUM_HASH_LENGTH               32
#define ANCHOR_HASH_LENGTH                     32

/**
 * Token and minting policy sizes.
 */
#define MINTING_POLICY_ID_SIZE 28
#define ASSET_NAME_SIZE_MAX    32

/**
 * Reward account size (in bytes).
 * Format: 1 byte header + 28 bytes key hash = 29 bytes
 */
#define REWARD_ACCOUNT_SIZE (1 + ADDRESS_KEY_HASH_LENGTH)

/**
 * Maximum address sizes (in bytes).
 * Shelley addresses: up to 1 (header) + 28 (payment) + 28 (staking) = 57 bytes
 * Byron addresses: up to 100+ bytes
 */
#define MAX_ADDRESS_SIZE              128
#define MAX_HUMAN_ADDRESS_SIZE        150
#define MAX_HUMAN_REWARD_ACCOUNT_SIZE 65

/**
 * Network IDs and protocol magic numbers for different Cardano networks.
 * Network ID is 4-bit value embedded in address headers.
 * Protocol magic is 32-bit value used in Byron addresses.
 */
#define MAINNET_NETWORK_ID     1
#define MAINNET_PROTOCOL_MAGIC 764824073

#define TESTNET_NETWORK_ID             0
#define TESTNET_PROTOCOL_MAGIC_LEGACY  1097911063
#define TESTNET_PROTOCOL_MAGIC_PREPROD 1
#define TESTNET_PROTOCOL_MAGIC_PREVIEW 2

/**
 * Maximum valid network ID (4 bits = 0-15).
 */
#define MAXIMUM_NETWORK_ID 0b1111

/**
 * URL and domain name constraints for certificate metadata and pool information.
 */
#define ANCHOR_URL_LENGTH_MAX       128
#define POOL_METADATA_URL_LENGTH_MAX 128
#define DNS_NAME_SIZE_MAX           128

/**
 * IP address sizes for pool relay information.
 */
#define IPV4_SIZE 4
#define IPV6_SIZE 16

/**
 * Pool margin denominator maximum (for fractional operator margins).
 * Used in UI display calculations. See ui_displayMarginScreen().
 * Represents margin as a fraction: actual_margin = margin_value / MARGIN_DENOMINATOR_MAX
 */
#define MARGIN_DENOMINATOR_MAX 1000000000000000ul  // 10^15

/**
 * Maximum native script nesting depth.
 * A depth of n means the script can handle up to n-1 levels of nesting.
 * This is an application-specific limit to prevent stack overflow.
 */
#define MAX_SCRIPT_DEPTH 11
