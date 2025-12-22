#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cardano_constants.h"
#include "addressUtils/addressUtilsShelley.h"

// Certificate body types (matches CDDL)
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

// Relay definitions used by pool registration certificates
typedef enum {
    RELAY_SINGLE_HOST_IP = 0,
    RELAY_SINGLE_HOST_NAME = 1,
    RELAY_MULTIPLE_HOST_NAME = 2
} relay_format_t;

typedef struct {
    bool isNull;
    uint8_t ip[IPV4_SIZE];
} ipv4_t;

typedef struct {
    bool isNull;
    uint8_t ip[IPV6_SIZE];
} ipv6_t;

typedef struct {
    bool isNull;
    uint16_t number;
} ipport_t;

typedef struct {
    relay_format_t format;
    ipport_t port;
    ipv4_t ipv4;
    ipv6_t ipv6;
    size_t dnsNameSize;
    uint8_t dnsName[DNS_NAME_SIZE_MAX];
} pool_relay_t;

typedef struct {
    key_reference_type_t keyReferenceType;
    union {
        bip44_path_t path;
        uint8_t hash[POOL_KEY_HASH_LENGTH];
    };
} pool_id_t;

typedef struct {
    key_reference_type_t keyReferenceType;
    union {
        bip44_path_t path;
        uint8_t keyHash[ADDRESS_KEY_HASH_LENGTH];
    };
} pool_owner_t;

typedef struct {
    uint8_t url[POOL_METADATA_URL_LENGTH_MAX];
    size_t urlSize;
    uint8_t hash[POOL_METADATA_HASH_LENGTH];
} pool_metadata_t;

// Anchor structures for committee/DRep certificates
typedef struct {
    bool isIncluded;
    uint8_t url[ANCHOR_URL_LENGTH_MAX];
    size_t urlLength;
    uint8_t hash[ANCHOR_HASH_LENGTH];
} anchor_t;

// Governing action identifiers
typedef struct {
    uint8_t txHashBuffer[TX_HASH_LENGTH];
    uint32_t govActionIndex;
} gov_action_id_t;

// Voting support types
typedef enum {
    VOTER_COMMITTEE_HOT_KEY_HASH = 0,
    VOTER_COMMITTEE_HOT_SCRIPT_HASH = 1,
    VOTER_DREP_KEY_HASH = 2,
    VOTER_DREP_SCRIPT_HASH = 3,
    VOTER_STAKE_POOL_KEY_HASH = 4,
} voter_type_t;

typedef struct {
    voter_type_t type;
    union {
        uint8_t keyHash[ADDRESS_KEY_HASH_LENGTH];
        uint8_t scriptHash[SCRIPT_HASH_LENGTH];
    };
} voter_t;

typedef enum {
    EXT_VOTER_COMMITTEE_HOT_KEY_HASH = 0,
    EXT_VOTER_COMMITTEE_HOT_KEY_PATH = 0 + 100,
    EXT_VOTER_COMMITTEE_HOT_SCRIPT_HASH = 1,
    EXT_VOTER_DREP_KEY_HASH = 2,
    EXT_VOTER_DREP_KEY_PATH = 2 + 100,
    EXT_VOTER_DREP_SCRIPT_HASH = 3,
    EXT_VOTER_STAKE_POOL_KEY_HASH = 4,
    EXT_VOTER_STAKE_POOL_KEY_PATH = 4 + 100,
} ext_voter_type_t;

typedef struct {
    ext_voter_type_t type;
    union {
        bip44_path_t keyPath;
        uint8_t keyHash[ADDRESS_KEY_HASH_LENGTH];
        uint8_t scriptHash[SCRIPT_HASH_LENGTH];
    };
} ext_voter_t;

typedef enum {
    VOTE_NO = 0,
    VOTE_YES = 1,
    VOTE_ABSTAIN = 2,
} vote_t;

typedef struct {
    vote_t vote;
    anchor_t anchor;
} voting_procedure_t;
