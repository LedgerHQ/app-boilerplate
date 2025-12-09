#pragma once

#include "memory/flist.h"
#include "txHashBuilder/txHashBuilder.h"
#include "constants.h"  // For hash and policy ID constants

// Maximum limits for output elements
#define MAX_ASSET_GROUPS_PER_OUTPUT 10
#define MAX_TOKENS_PER_ASSET_GROUP 20
#define MAX_DATUM_INLINE_SIZE 256
#define MAX_REF_SCRIPT_SIZE 512
#define ASSET_NAME_HASH_SIZE 32  // Blake2b-256 hash for asset name
#define ASSET_NAME_DISPLAY_SIZE 32  // Maximum asset name length for display

// Token within an asset group
// Note: named output_token_t to avoid conflict with cbor_token_t alias token_t
typedef struct {
    uint8_t assetNameHash[ASSET_NAME_HASH_SIZE];  // Blake2b-224 hash of asset name (28 bytes used)
    uint8_t assetNameLen;                          // Length of original asset name (0-32)
    uint8_t assetName[ASSET_NAME_DISPLAY_SIZE];   // Original asset name (for display)
    int64_t amount;                                // Can be positive (mint) or negative (burn)
} output_token_t;

// Asset group (policy ID + tokens)
typedef struct {
    uint8_t policyId[MINTING_POLICY_ID_SIZE];     // Policy ID
    uint16_t numTokens;                            // Number of tokens in this group
    output_token_t* tokens;                        // Dynamically allocated array of tokens
} asset_group_t;

// Datum structure - contains union of hash and inline data
// Uses datum_type_t values from txHashBuilder for compatibility
// Wire format: 0=NONE, 1=HASH, 2=INLINE
// Internal format: 0xFF=NONE (marker), 0=DATUM_HASH, 1=DATUM_INLINE
typedef struct {
    datum_type_t type;                             // Internal: 0=HASH, 1=INLINE (from txHashBuilder), or 0xFF=NONE
    union {
        uint8_t hash[OUTPUT_DATUM_HASH_LENGTH];    // For DATUM_HASH
        struct {
            uint16_t size;
            uint8_t* data;      // Dynamically allocated for DATUM_INLINE
        } inline_data;
    };
} output_datum_t;

// Helper macro to check if datum is present
#define OUTPUT_DATUM_IS_PRESENT(datum) ((datum).type != 0xFF)

// Reference script structure
typedef struct {
    uint16_t size;
    uint8_t* data;              // Dynamically allocated
} ref_script_t;

// Output list item with flist node
// Contains destination, amount, tokens, datum, ref script, and format
typedef struct {
    s_flist_node node;      /// flist node for linked list
    struct {
        tx_output_destination_storage_t destination;
        uint64_t adaAmount;

        // Token bundle
        uint16_t numAssetGroups;
        asset_group_t* assetGroups;  // Dynamically allocated array

        // Datum
        output_datum_t datum;

        // Reference script
        bool hasRefScript;
        ref_script_t refScript;

        // Output format
        tx_output_serialization_format_t format;  /// ARRAY_LEGACY or MAP_BABBAGE
    } output_data;
} tx_output_list_item_t;
