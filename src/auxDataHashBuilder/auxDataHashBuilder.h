#pragma once

#if defined(TEST)
#include "hash.h"
#else
#include "crypto/hash.h"
#endif
#include "keyDerivation/keyDerivation.h"
#include "utils/utils.h"

enum {
    METADATA_KEY_CVOTE_REGISTRATION_PAYLOAD = 61284,
    METADATA_KEY_CVOTE_REGISTRATION_SIGNATURE = 61285,
};

enum {
    CVOTE_REGISTRATION_PAYLOAD_KEY_VOTE_KEY = 1,
    CVOTE_REGISTRATION_PAYLOAD_KEY_STAKING_KEY = 2,
    CVOTE_REGISTRATION_PAYLOAD_PAYMENT_ADDRESS = 3,
    CVOTE_REGISTRATION_PAYLOAD_KEY_NONCE = 4,
    CVOTE_REGISTRATION_PAYLOAD_VOTING_PURPOSE = 5,
};

enum {
    CVOTE_REGISTRATION_SIGNATURE_KEY = 1,
};

typedef enum {
    AUX_DATA_HASH_BUILDER_INIT = 100,
    AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_INIT = 200,
    AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_INIT = 210,
    AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_VOTE_KEY = 211,
    AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_DELEGATIONS = 212,
    AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_STAKING_KEY = 213,
    AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_PAYMENT_ADDRESS = 214,
    AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_NONCE = 215,
    AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_VOTING_PURPOSE = 216,
    AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_SIGNATURE = 220,
    AUX_DATA_HASH_BUILDER_IN_AUXILIARY_SCRIPTS = 300,
    AUX_DATA_HASH_BUILDER_FINISHED = 400,
} aux_data_hash_builder_state_t;

typedef enum { CIP15 = 1, CIP36 = 2 } cvote_registration_format_t;

typedef struct {
    struct {
        blake2b_256_context_t payloadHash;
        cvote_registration_format_t format;
        uint16_t remainingDelegations;
    } cVoteRegistrationData;

    aux_data_hash_builder_state_t state;
    blake2b_256_context_t auxDataHash;
} aux_data_hash_builder_t;

void auxDataHashBuilder_init(aux_data_hash_builder_t* builder);

void auxDataHashBuilder_cVoteRegistration_enter(aux_data_hash_builder_t* builder,
                                                cvote_registration_format_t format);
void auxDataHashBuilder_cVoteRegistration_enterPayload(aux_data_hash_builder_t* builder);
void auxDataHashBuilder_cVoteRegistration_addVoteKey(aux_data_hash_builder_t* builder,
                                                     const uint8_t* votePubKeyBuffer,
                                                     size_t votePubKeySize);
void auxDataHashBuilder_cVoteRegistration_enterDelegations(aux_data_hash_builder_t* builder,
                                                           size_t numDelegations);
void auxDataHashBuilder_cVoteRegistration_addDelegation(aux_data_hash_builder_t* builder,
                                                        const uint8_t* votePubKeyBuffer,
                                                        size_t votePubKeySize,
                                                        uint32_t weight);
void auxDataHashBuilder_cVoteRegistration_addStakingKey(aux_data_hash_builder_t* builder,
                                                        const uint8_t* stakingPubKeyBuffer,
                                                        size_t stakingPubKeySize);
void auxDataHashBuilder_cVoteRegistration_addPaymentAddress(aux_data_hash_builder_t* builder,
                                                            const uint8_t* addressBuffer,
                                                            size_t addressSize);
void auxDataHashBuilder_cVoteRegistration_addNonce(aux_data_hash_builder_t* builder,
                                                   uint64_t nonce);
void auxDataHashBuilder_cVoteRegistration_addVotingPurpose(aux_data_hash_builder_t* builder,
                                                           uint64_t votingPurpose);
void auxDataHashBuilder_cVoteRegistration_finalizePayload(aux_data_hash_builder_t* builder,
                                                          uint8_t* outBuffer,
                                                          size_t outSize);

void auxDataHashBuilder_cVoteRegistration_addSignature(aux_data_hash_builder_t* builder,
                                                       const uint8_t* signatureBuffer,
                                                       size_t signatureSize);

void auxDataHashBuilder_cVoteRegistration_addAuxiliaryScripts(aux_data_hash_builder_t* builder);

void auxDataHashBuilder_finalize(aux_data_hash_builder_t* builder,
                                 uint8_t* outBuffer,
                                 size_t outSize);
