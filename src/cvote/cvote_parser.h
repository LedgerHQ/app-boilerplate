#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "addressUtils/addressUtilsShelley.h"
#include "buffer.h"
#include "cardano_constants.h"
#include "cvote/aux_data_hash_builder.h"
#include "transaction/tx_credential_types.h"

typedef enum {
    CVOTE_PARSER_OK = 0,
    CVOTE_PARSER_INVALID_FORMAT,
    CVOTE_PARSER_OUT_OF_MEMORY,
} cvote_parser_status_t;

#define CVOTE_PUBLIC_KEY_LENGTH (PUBLIC_KEY_SIZE)

typedef struct {
    uint8_t *buffer;
    size_t length;
} cvote_third_party_address_t;

typedef struct {
    bool is_third_party;
    union {
        addressParams_t params;
        cvote_third_party_address_t third_party;
    };
} cvote_destination_t;

typedef struct {
    cvote_registration_format_t format;
    uint16_t delegation_count;
    ext_credential_t staking_credential;
    cvote_destination_t destination;
    uint64_t nonce;
    bool has_voting_purpose;
    uint64_t voting_purpose;
    bool has_vote_credential;
    ext_credential_t vote_credential;
    bool final_fields_processed;
    aux_data_hash_builder_t hash_builder;
    uint8_t registration_signature[ED25519_SIGNATURE_LENGTH];
} cvote_aux_data_t;

cvote_parser_status_t cvote_parse_credential(buffer_t *buf,
                                             ext_credential_t *credential,
                                             const char *label);
cvote_parser_status_t cvote_parse_destination(buffer_t *buf, cvote_destination_t *destination);
cvote_parser_status_t cvote_parse_aux_data_init(buffer_t *buf, cvote_aux_data_t **out_data);
