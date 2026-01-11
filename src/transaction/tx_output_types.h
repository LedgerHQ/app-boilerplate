#pragma once

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cardano_constants.h"
#include "memory/flist.h"
#include "addressUtils/addressUtilsShelley.h"

#define MAX_DATUM_INLINE_LENGTH INT16_MAX  // TODO: inline datum length currently bounded only by wire/data limits (effectively unlimited)
#define MAX_REF_SCRIPT_LENGTH INT16_MAX    // TODO: reference script length currently bounded only by wire/data limits (effectively unlimited)
#define ASSET_NAME_HASH_SIZE 32
#define ASSET_NAME_DISPLAY_SIZE 32

typedef enum {
    DESTINATION_THIRD_PARTY = 1,
    DESTINATION_DEVICE_OWNED = 2,
} tx_output_destination_type_t;

typedef struct {
    tx_output_destination_type_t type;
    union {
        struct {
            const uint8_t* buffer;
            size_t size;
        } address;
        addressParams_t params;
    };
} tx_output_destination_storage_t;

typedef struct {
    tx_output_destination_type_t type;
    union {
        struct {
            const uint8_t* buffer;
            size_t size;
        } address;
        addressParams_t* params;
    };
} tx_output_destination_t;

typedef struct {
    const uint8_t* assetName;
    uint8_t assetNameLen;
    uint64_t amount;
} output_token_t;

typedef struct {
    s_flist_node flist_node;
    output_token_t token_data;
} output_token_node_t;

typedef struct {
    const uint8_t* policyId;
    uint16_t numTokens;
    s_flist_node* tokens;
} output_asset_group_t;

typedef struct {
    s_flist_node flist_node;
    output_asset_group_t asset_group;
} output_asset_group_node_t;

typedef enum {
    DATUM_HASH = 0,
    DATUM_INLINE = 1,
} datum_type_t;

typedef struct {
    bool hasDatum;
    datum_type_t type;
    union {
        const uint8_t* hash;  // Points to 32-byte hash in raw_tx buffer
        struct {
            uint16_t size;
            const uint8_t* data;  // Points to data in raw_tx buffer
        } inline_data;
    };
} output_datum_t;

typedef struct {
    bool hasRefScript;
    uint16_t size;
    const uint8_t* data;  // Points to data in raw_tx buffer
} ref_script_t;

typedef enum {
    ARRAY_LEGACY = 0,
    MAP_BABBAGE = 1
} tx_output_serialization_format_t;

typedef struct {
    s_flist_node flist_node;
    struct {
        tx_output_destination_storage_t destination;
        uint64_t adaAmount;
        uint16_t numAssetGroups;
        s_flist_node* assetGroups;
        output_datum_t datum;
        ref_script_t refScript;
        tx_output_serialization_format_t format;
    } output_data;
} tx_output_node_t;
