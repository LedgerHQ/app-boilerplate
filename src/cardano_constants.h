#pragma once

/**
 * Lovelace (ADA in smallest units) limits.
 */
#define LOVELACE_MAX_SUPPLY 45000000000000000
#define LOVELACE_INVALID    47000000000000000

/**
 * Lengths of cryptographic material.
 */
#define ED25519_SIGNATURE_LENGTH               64
#define ED25519_EXTENDED_PRIVKEY_LENGTH        64
#define ED25519_PUBKEY_UNCOMPRESSED_LENGTH     65
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
 * Native token policy constants.
 */
#define MINTING_POLICY_ID_LENGTH 28
#define MAX_ASSET_NAME_LENGTH    32

/**
 * Reward account serialization.
 */
#define REWARD_ACCOUNT_LENGTH (1 + ADDRESS_KEY_HASH_LENGTH)

/**
 * Network IDs and protocol magics.
 */
#define MAINNET_NETWORK_ID     1
#define MAINNET_PROTOCOL_MAGIC 764824073

#define TESTNET_NETWORK_ID             0
#define TESTNET_PROTOCOL_MAGIC_LEGACY  1097911063
#define TESTNET_PROTOCOL_MAGIC_PREPROD 1
#define TESTNET_PROTOCOL_MAGIC_PREVIEW 2

#define MAXIMUM_NETWORK_ID 0xF

/**
 * Relay metadata lengths.
 */
#define ANCHOR_URL_LENGTH_MAX       128
#define POOL_METADATA_URL_LENGTH_MAX 128
#define MAX_DNS_NAME_LENGTH         128

/**
 * IP address storage lengths.
 */
#define IPV4_LENGTH 4
#define IPV6_LENGTH 16

/**
 * Maximum allowed pool margin denominator.
 */
#define MARGIN_DENOMINATOR_MAX 1000000000000000ul

/**
 * Native script depth limit.
 */
#define SCRIPT_DEPTH_MAX 11
