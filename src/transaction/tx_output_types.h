#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cardano_constants.h"
#include "memory/flist.h"
#include "addressUtils/addressUtilsShelley.h"

#define MAX_ASSET_GROUPS_PER_OUTPUT 10
#define MAX_TOKENS_PER_ASSET_GROUP 20
#define MAX_DATUM_INLINE_LENGTH 256
#define MAX_REF_SCRIPT_LENGTH 512
#define ASSET_NAME_HASH_SIZE 32
#define ASSET_NAME_DISPLAY_SIZE 32

typedef struct {
    uint8_t policyId[MINTING_POLICY_ID_LENGTH];
} token_group_t;

typedef struct {
    uint8_t assetNameBytes[MAX_ASSET_NAME_LENGTH];
    size_t assetNameSize;
    uint64_t amount;
} output_token_amount_t;

typedef enum {
    ARRAY_LEGACY = 0,
    MAP_BABBAGE = 1
} tx_output_serialization_format_t;

typedef enum {
    DATUM_HASH = 0,
    DATUM_INLINE = 1,
} datum_type_t;

typedef enum {
    DESTINATION_THIRD_PARTY = 1,
    DESTINATION_DEVICE_OWNED = 2,
} tx_output_destination_type_t;

typedef struct {
    tx_output_destination_type_t type;
    union {
        struct {
            uint8_t buffer[MAX_ADDRESS_LENGTH];
            size_t size;
        } address;
        addressParams_t params;
    };
} tx_output_destination_storage_t;

typedef struct {
    tx_output_destination_type_t type;
    union {
        struct {
            uint8_t* buffer;
            size_t size;
        } address;
        addressParams_t* params;
    };
} tx_output_destination_t;

typedef struct {
    uint8_t assetNameHash[ASSET_NAME_HASH_SIZE];
    uint8_t assetNameLen;
    uint8_t assetName[ASSET_NAME_DISPLAY_SIZE];
    int64_t amount;
} output_token_t;

typedef struct {
    uint8_t policyId[MINTING_POLICY_ID_LENGTH];
    uint16_t numTokens;
    output_token_t* tokens;
} asset_group_t;

typedef struct {
    datum_type_t type;
    union {
        uint8_t hash[OUTPUT_DATUM_HASH_LENGTH];
        struct {
            uint16_t size;
            uint8_t* data;
        } inline_data;
    };
} output_datum_t;

typedef struct {
    uint16_t size;
    uint8_t* data;
} ref_script_t;

typedef struct {
    s_flist_node node;
    struct {
        tx_output_destination_storage_t destination;
        uint64_t adaAmount;
        uint16_t numAssetGroups;
        asset_group_t* assetGroups;
        output_datum_t datum;
        bool hasRefScript;
        ref_script_t refScript;
        tx_output_serialization_format_t format;
    } output_data;
} tx_output_list_item_t;
