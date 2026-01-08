#include "cvote/aux_data_hash_builder.h"

#include "cardano_constants.h"
#include "cbor.h"
#include "utils/utils.h"
#include <string.h>

enum {
    HC_AUX_DATA = (1u << 0),
    HC_CVOTE_REGISTRATION_PAYLOAD = (1u << 1),
};

/*
 * Optional tracing for debugging aux data hash serialization.
 * Enabled via -DTRACE_TX_HASH_BUILDER to capture the exact CBOR bytes
 * being hashed.
 */
#ifdef TRACE_AUX_DATA_HASH_BUILDER
enum {
    AUX_DATA_TRACE_BUFFER_SIZE = 2 * 1024,
    CVOTE_PAYLOAD_TRACE_BUFFER_SIZE = 3 * 1024,
};
static uint8_t aux_data_hash_trace_buffer[AUX_DATA_TRACE_BUFFER_SIZE];
static size_t aux_data_hash_trace_size = 0;
static uint8_t cvote_payload_trace_buffer[CVOTE_PAYLOAD_TRACE_BUFFER_SIZE];
static size_t cvote_payload_trace_size = 0;

static void auxDataHashBuilder_trace_record(uint8_t hashContexts,
                                            const uint8_t* buffer,
                                            size_t size)
{
    ASSERT(buffer != NULL);
    if (hashContexts & HC_AUX_DATA) {
        ASSERT(aux_data_hash_trace_size + size <= AUX_DATA_TRACE_BUFFER_SIZE);
        memcpy(aux_data_hash_trace_buffer + aux_data_hash_trace_size, buffer, size);
        aux_data_hash_trace_size += size;
    }
    if (hashContexts & HC_CVOTE_REGISTRATION_PAYLOAD) {
        ASSERT(cvote_payload_trace_size + size <= CVOTE_PAYLOAD_TRACE_BUFFER_SIZE);
        memcpy(cvote_payload_trace_buffer + cvote_payload_trace_size, buffer, size);
        cvote_payload_trace_size += size;
    }
}
#define _TRACE(...) TRACE(__VA_ARGS__)
#else
#define auxDataHashBuilder_trace_record(hashContexts, buffer, size) ((void)0)
#define _TRACE(...)
#endif

#define APPEND_CBOR(hashContexts, type, value) \
    auxDataHashBuilder_append_cbor(builder, hashContexts, type, value)

#define APPEND_DATA(hashContexts, buffer, bufferSize) \
    auxDataHashBuilder_append_buffer(builder, hashContexts, buffer, bufferSize)

static void auxDataHashBuilder_append_cbor(aux_data_hash_builder_t* builder,
                                           uint8_t hashContexts,
                                           uint8_t type,
                                           uint64_t value) {
    uint8_t buffer[10] = {0};
    size_t size = 0;
    ASSERT(cbor_writeToken(type, value, buffer, SIZEOF(buffer), &size));
    auxDataHashBuilder_trace_record(hashContexts, buffer, size);
    if (hashContexts & HC_AUX_DATA) {
        blake2b_256_append(&builder->auxDataHash, buffer, size);
    }
    if (hashContexts & HC_CVOTE_REGISTRATION_PAYLOAD) {
        blake2b_256_append(&builder->cVoteRegistrationData.payloadHash, buffer, size);
    }
}

static void auxDataHashBuilder_append_buffer(aux_data_hash_builder_t* builder,
                                             uint8_t hashContexts,
                                             const uint8_t* buffer,
                                             size_t bufferSize) {
    ASSERT(buffer != NULL);
    ASSERT(bufferSize < BUFFER_SIZE_PARANOIA);
    auxDataHashBuilder_trace_record(hashContexts, buffer, bufferSize);
    if (hashContexts & HC_AUX_DATA) {
        blake2b_256_append(&builder->auxDataHash, buffer, bufferSize);
    }
    if (hashContexts & HC_CVOTE_REGISTRATION_PAYLOAD) {
        blake2b_256_append(&builder->cVoteRegistrationData.payloadHash, buffer, bufferSize);
    }
}

void auxDataHashBuilder_init(aux_data_hash_builder_t* builder) {
    _TRACE("Serializing tx auxiliary data");
    blake2b_256_init(&builder->auxDataHash);
    blake2b_256_init(&builder->cVoteRegistrationData.payloadHash);

#ifdef TRACE_AUX_DATA_HASH_BUILDER
    aux_data_hash_trace_size = 0;
    cvote_payload_trace_size = 0;
#endif

    APPEND_CBOR(HC_AUX_DATA, CBOR_TYPE_ARRAY, 2);
    builder->state = AUX_DATA_HASH_BUILDER_INIT;
}

void auxDataHashBuilder_cVoteRegistration_enter(aux_data_hash_builder_t* builder,
                                                cvote_registration_format_t format) {
    _TRACE("state = %d", builder->state);

    ASSERT(format == CIP15 || format == CIP36);
    builder->cVoteRegistrationData.format = format;

    ASSERT(builder->state == AUX_DATA_HASH_BUILDER_INIT);
    APPEND_CBOR(HC_AUX_DATA, CBOR_TYPE_MAP, 2);
    APPEND_CBOR(HC_CVOTE_REGISTRATION_PAYLOAD, CBOR_TYPE_MAP, 1);
    builder->state = AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_INIT;
}

void auxDataHashBuilder_cVoteRegistration_enterPayload(aux_data_hash_builder_t* builder) {
    _TRACE("state = %d", builder->state);

    ASSERT(builder->state == AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_INIT);
    size_t mapSize = (builder->cVoteRegistrationData.format == CIP36) ? 5 : 4;
    APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD,
                CBOR_TYPE_UNSIGNED,
                METADATA_KEY_CVOTE_REGISTRATION_PAYLOAD);
    APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD, CBOR_TYPE_MAP, mapSize);
    builder->state = AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_INIT;
}

void auxDataHashBuilder_cVoteRegistration_addVoteKey(aux_data_hash_builder_t* builder,
                                                     const uint8_t* votePubKeyBuffer,
                                                     size_t votePubKeySize) {
    _TRACE("state = %d", builder->state);

    ASSERT(votePubKeySize < BUFFER_SIZE_PARANOIA);
    ASSERT(builder->state == AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_INIT);

    APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD,
                CBOR_TYPE_UNSIGNED,
                CVOTE_REGISTRATION_PAYLOAD_KEY_VOTE_KEY);
    ASSERT(votePubKeySize == PUBLIC_KEY_SIZE);
    APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD, CBOR_TYPE_BYTES, votePubKeySize);
    APPEND_DATA(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD, votePubKeyBuffer, votePubKeySize);

    builder->state = AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_VOTE_KEY;
}

void auxDataHashBuilder_cVoteRegistration_enterDelegations(aux_data_hash_builder_t* builder,
                                                           size_t numDelegations) {
    _TRACE("state = %d", builder->state);

    builder->cVoteRegistrationData.remainingDelegations = numDelegations;

    ASSERT(builder->state == AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_INIT);
    {
        APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD,
                    CBOR_TYPE_UNSIGNED,
                    CVOTE_REGISTRATION_PAYLOAD_KEY_VOTE_KEY);
        APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD, CBOR_TYPE_ARRAY, numDelegations);
    }
    builder->state = AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_DELEGATIONS;
}

void auxDataHashBuilder_cVoteRegistration_addDelegation(aux_data_hash_builder_t* builder,
                                                        const uint8_t* votePubKeyBuffer,
                                                        size_t votePubKeySize,
                                                        uint32_t weight) {
    _TRACE("state = %d", builder->state);

    ASSERT(votePubKeySize < BUFFER_SIZE_PARANOIA);

    ASSERT(builder->state == AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_DELEGATIONS);
    ASSERT(builder->cVoteRegistrationData.remainingDelegations > 0);

    builder->cVoteRegistrationData.remainingDelegations--;

    {
        APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD, CBOR_TYPE_ARRAY, 2);
        {
            ASSERT(votePubKeySize == PUBLIC_KEY_SIZE);
            APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD,
                        CBOR_TYPE_BYTES,
                        votePubKeySize);
            APPEND_DATA(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD,
                        votePubKeyBuffer,
                        votePubKeySize);
        }
        APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD, CBOR_TYPE_UNSIGNED, weight);
    }
}

void auxDataHashBuilder_cVoteRegistration_addStakingKey(aux_data_hash_builder_t* builder,
                                                        const uint8_t* stakingPubKeyBuffer,
                                                        size_t stakingPubKeySize) {
    _TRACE("state = %d", builder->state);

    ASSERT(stakingPubKeySize < BUFFER_SIZE_PARANOIA);

    switch (builder->state) {
        case AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_VOTE_KEY:
            // ok
            break;

        case AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_DELEGATIONS:
            ASSERT(builder->cVoteRegistrationData.remainingDelegations == 0);
            break;

        default:
            // should not happen
            ASSERT(false);
    }

    {
        APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD,
                    CBOR_TYPE_UNSIGNED,
                    CVOTE_REGISTRATION_PAYLOAD_KEY_STAKING_KEY);
        {
            ASSERT(stakingPubKeySize == PUBLIC_KEY_SIZE);
            APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD,
                        CBOR_TYPE_BYTES,
                        stakingPubKeySize);
            APPEND_DATA(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD,
                        stakingPubKeyBuffer,
                        stakingPubKeySize);
        }
    }
    builder->state = AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_STAKING_KEY;
}

void auxDataHashBuilder_cVoteRegistration_addPaymentAddress(aux_data_hash_builder_t* builder,
                                                            const uint8_t* addressBuffer,
                                                            size_t addressSize) {
    _TRACE("state = %d", builder->state);

    ASSERT(addressSize > 0);
    ASSERT(addressSize < BUFFER_SIZE_PARANOIA);

    ASSERT(builder->state == AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_STAKING_KEY);
    {
        APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD,
                    CBOR_TYPE_UNSIGNED,
                    CVOTE_REGISTRATION_PAYLOAD_PAYMENT_ADDRESS);
        {
            APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD, CBOR_TYPE_BYTES, addressSize);
            APPEND_DATA(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD, addressBuffer, addressSize);
        }
    }
    builder->state = AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_PAYMENT_ADDRESS;
}

void auxDataHashBuilder_cVoteRegistration_addNonce(aux_data_hash_builder_t* builder,
                                                   uint64_t nonce) {
    _TRACE("state = %d", builder->state);

    ASSERT(builder->state == AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_PAYMENT_ADDRESS);
    {
        APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD,
                    CBOR_TYPE_UNSIGNED,
                    CVOTE_REGISTRATION_PAYLOAD_KEY_NONCE);
        APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD, CBOR_TYPE_UNSIGNED, nonce);
    }
    builder->state = AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_NONCE;
}

void auxDataHashBuilder_cVoteRegistration_addVotingPurpose(aux_data_hash_builder_t* builder,
                                                           uint64_t votingPurpose) {
    _TRACE("state = %d", builder->state);

    ASSERT(builder->state == AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_NONCE);
    {
        APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD,
                    CBOR_TYPE_UNSIGNED,
                    CVOTE_REGISTRATION_PAYLOAD_VOTING_PURPOSE);
        APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD, CBOR_TYPE_UNSIGNED, votingPurpose);
    }
    builder->state = AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_VOTING_PURPOSE;
}

void auxDataHashBuilder_cVoteRegistration_finalizePayload(aux_data_hash_builder_t* builder,
                                                          uint8_t* outBuffer,
                                                          size_t outSize) {
    _TRACE("state = %d", builder->state);

    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    ASSERT(builder->state == AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_NONCE ||
           builder->state == AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_VOTING_PURPOSE);

    ASSERT(outSize == CVOTE_REGISTRATION_PAYLOAD_HASH_LENGTH);
    { blake2b_256_finalize(&builder->cVoteRegistrationData.payloadHash, outBuffer, outSize); }
}

void auxDataHashBuilder_cVoteRegistration_addSignature(aux_data_hash_builder_t* builder,
                                                       const uint8_t* signatureBuffer,
                                                       size_t signatureSize) {
    _TRACE("state = %d", builder->state);

    ASSERT(signatureSize < BUFFER_SIZE_PARANOIA);

    ASSERT(builder->state == AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_NONCE ||
           builder->state == AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_VOTING_PURPOSE);
    {
        APPEND_CBOR(HC_AUX_DATA, CBOR_TYPE_UNSIGNED, METADATA_KEY_CVOTE_REGISTRATION_SIGNATURE);
        {
            ASSERT(signatureSize == ED25519_SIGNATURE_LENGTH);
            APPEND_CBOR(HC_AUX_DATA, CBOR_TYPE_MAP, 1);
            APPEND_CBOR(HC_AUX_DATA, CBOR_TYPE_UNSIGNED, CVOTE_REGISTRATION_SIGNATURE_KEY);
            APPEND_CBOR(HC_AUX_DATA, CBOR_TYPE_BYTES, signatureSize);
            APPEND_DATA(HC_AUX_DATA, signatureBuffer, signatureSize);
        }
    }
    builder->state = AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_SIGNATURE;
}

void auxDataHashBuilder_cVoteRegistration_addAuxiliaryScripts(aux_data_hash_builder_t* builder) {
    _TRACE("state = %d", builder->state);

    ASSERT(builder->state == AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_SIGNATURE);
    {
        // auxiliary scripts currently hard-coded to an empty list
        APPEND_CBOR(HC_AUX_DATA, CBOR_TYPE_ARRAY, 0);
    }

    builder->state = AUX_DATA_HASH_BUILDER_IN_AUXILIARY_SCRIPTS;
}

void auxDataHashBuilder_finalize(aux_data_hash_builder_t* builder,
                                 uint8_t* outBuffer,
                                 size_t outSize) {
    _TRACE("state = %d", builder->state);

    ASSERT(builder->state == AUX_DATA_HASH_BUILDER_IN_AUXILIARY_SCRIPTS);

    ASSERT(outSize == AUX_DATA_HASH_LENGTH);

    blake2b_256_finalize(&builder->auxDataHash, outBuffer, outSize);
#ifdef TRACE_AUX_DATA_HASH_BUILDER
    TRACE("aux data cbor (%u bytes)", (unsigned) aux_data_hash_trace_size);
    TRACE_BUFFER(aux_data_hash_trace_buffer, aux_data_hash_trace_size);
    TRACE("cvote payload cbor (%u bytes)", (unsigned) cvote_payload_trace_size);
    TRACE_BUFFER(cvote_payload_trace_buffer, cvote_payload_trace_size);
#endif
    builder->state = AUX_DATA_HASH_BUILDER_FINISHED;
}
