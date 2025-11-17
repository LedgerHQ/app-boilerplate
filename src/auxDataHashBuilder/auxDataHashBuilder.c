#include "auxDataHashBuilder/auxDataHashBuilder.h"

#include "cbor.h"
#include "utils/utils.h"

// this tracing is rarely needed so we avoid polluting the log by default
//#define TRACE_AUX_DATA_HASH_BUILDER

#ifdef TRACE_AUX_DATA_HASH_BUILDER
#define _TRACE(...) TRACE(__VA_ARGS__)
#else
#define _TRACE(...)
#endif

enum {
    HC_AUX_DATA = (1u << 0),
    HC_CVOTE_REGISTRATION_PAYLOAD = (1u << 1),
};

#define APPEND_CBOR(hashContexts, type, value)                                           \
    do {                                                                                 \
        if ((hashContexts) & HC_AUX_DATA) {                                               \
            blake2b_256_append_cbor_aux_data(&builder->auxDataHash, type, value, true);  \
        }                                                                                \
        if ((hashContexts) & HC_CVOTE_REGISTRATION_PAYLOAD) {                            \
            blake2b_256_append_cbor_aux_data(&builder->cVoteRegistrationData.payloadHash, \
                                             type,                                        \
                                             value,                                       \
                                             false);                                      \
        }                                                                                \
    } while (0)

#define APPEND_DATA(hashContexts, buffer, bufferSize)                                      \
    do {                                                                                   \
        if ((hashContexts) & HC_AUX_DATA) {                                                 \
            blake2b_256_append_buffer_aux_data(&builder->auxDataHash, buffer, bufferSize, \
                                                 true);                                    \
        }                                                                                  \
        if ((hashContexts) & HC_CVOTE_REGISTRATION_PAYLOAD) {                              \
            blake2b_256_append_buffer_aux_data(&builder->cVoteRegistrationData.payloadHash, \
                                                 buffer,                                     \
                                                 bufferSize,                                 \
                                                 false);                                     \
        }                                                                                  \
    } while (0)

__noinline_due_to_stack__ static void blake2b_256_append_cbor_aux_data(
    blake2b_256_context_t* hashCtx,
    uint8_t type,
    uint64_t value,
    bool trace) {
    uint8_t buffer[10] = {0};
    size_t size = 0;
    ASSERT(cbor_writeToken(type, value, buffer, SIZEOF(buffer), &size));
    if (trace) {
        TRACE_BUFFER(buffer, size);
    }
    blake2b_256_append(hashCtx, buffer, size);
}

static void blake2b_256_append_buffer_aux_data(blake2b_256_context_t* hashCtx,
                                               const uint8_t* buffer,
                                               size_t bufferSize,
                                               bool trace) {
    ASSERT(bufferSize < BUFFER_SIZE_PARANOIA);

    if (trace) {
        TRACE_BUFFER(buffer, bufferSize);
    }
    blake2b_256_append(hashCtx, buffer, bufferSize);
}

void auxDataHashBuilder_init(aux_data_hash_builder_t* builder) {
    _TRACE("Serializing tx auxiliary data");
    blake2b_256_init(&builder->auxDataHash);
    blake2b_256_init(&builder->cVoteRegistrationData.payloadHash);

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

    APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD,
                CBOR_TYPE_UNSIGNED,
                CVOTE_REGISTRATION_PAYLOAD_KEY_VOTE_KEY);
    APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD, CBOR_TYPE_ARRAY, numDelegations);

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

    APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD, CBOR_TYPE_ARRAY, 2);
    ASSERT(votePubKeySize == PUBLIC_KEY_SIZE);
    APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD, CBOR_TYPE_BYTES, votePubKeySize);
    APPEND_DATA(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD, votePubKeyBuffer, votePubKeySize);
    APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD, CBOR_TYPE_UNSIGNED, weight);
}

void auxDataHashBuilder_cVoteRegistration_addStakingKey(aux_data_hash_builder_t* builder,
                                                        const uint8_t* stakingPubKeyBuffer,
                                                        size_t stakingPubKeySize) {
    _TRACE("state = %d", builder->state);

    ASSERT(stakingPubKeySize < BUFFER_SIZE_PARANOIA);

    switch (builder->state) {
        case AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_VOTE_KEY:
            break;
        case AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_DELEGATIONS:
            ASSERT(builder->cVoteRegistrationData.remainingDelegations == 0);
            break;
        default:
            ASSERT(false);
    }

    APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD,
                CBOR_TYPE_UNSIGNED,
                CVOTE_REGISTRATION_PAYLOAD_KEY_STAKING_KEY);
    ASSERT(stakingPubKeySize == PUBLIC_KEY_SIZE);
    APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD, CBOR_TYPE_BYTES, stakingPubKeySize);
    APPEND_DATA(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD,
                stakingPubKeyBuffer,
                stakingPubKeySize);

    builder->state = AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_STAKING_KEY;
}

void auxDataHashBuilder_cVoteRegistration_addPaymentAddress(aux_data_hash_builder_t* builder,
                                                            const uint8_t* addressBuffer,
                                                            size_t addressSize) {
    _TRACE("state = %d", builder->state);

    ASSERT(addressSize > 0);
    ASSERT(addressSize < BUFFER_SIZE_PARANOIA);
    ASSERT(builder->state == AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_STAKING_KEY);

    APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD,
                CBOR_TYPE_UNSIGNED,
                CVOTE_REGISTRATION_PAYLOAD_PAYMENT_ADDRESS);
    APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD, CBOR_TYPE_BYTES, addressSize);
    APPEND_DATA(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD, addressBuffer, addressSize);

    builder->state = AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_PAYMENT_ADDRESS;
}

void auxDataHashBuilder_cVoteRegistration_addNonce(aux_data_hash_builder_t* builder,
                                                   uint64_t nonce) {
    _TRACE("state = %d", builder->state);

    ASSERT(builder->state == AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_PAYMENT_ADDRESS);
    APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD,
                CBOR_TYPE_UNSIGNED,
                CVOTE_REGISTRATION_PAYLOAD_KEY_NONCE);
    APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD, CBOR_TYPE_UNSIGNED, nonce);
    builder->state = AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_NONCE;
}

void auxDataHashBuilder_cVoteRegistration_addVotingPurpose(aux_data_hash_builder_t* builder,
                                                           uint64_t votingPurpose) {
    _TRACE("state = %d", builder->state);

    ASSERT(builder->state == AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_NONCE);
    APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD,
                CBOR_TYPE_UNSIGNED,
                CVOTE_REGISTRATION_PAYLOAD_VOTING_PURPOSE);
    APPEND_CBOR(HC_AUX_DATA | HC_CVOTE_REGISTRATION_PAYLOAD, CBOR_TYPE_UNSIGNED, votingPurpose);
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

    blake2b_256_finalize(&builder->cVoteRegistrationData.payloadHash, outBuffer, outSize);
}

void auxDataHashBuilder_cVoteRegistration_addSignature(aux_data_hash_builder_t* builder,
                                                       const uint8_t* signatureBuffer,
                                                       size_t signatureSize) {
    _TRACE("state = %d", builder->state);

    ASSERT(signatureSize < BUFFER_SIZE_PARANOIA);
    ASSERT(builder->state == AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_NONCE ||
           builder->state == AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_PAYLOAD_VOTING_PURPOSE);

    APPEND_CBOR(HC_AUX_DATA, CBOR_TYPE_UNSIGNED, METADATA_KEY_CVOTE_REGISTRATION_SIGNATURE);
    APPEND_CBOR(HC_AUX_DATA, CBOR_TYPE_MAP, 1);
    APPEND_CBOR(HC_AUX_DATA, CBOR_TYPE_UNSIGNED, CVOTE_REGISTRATION_SIGNATURE_KEY);
    ASSERT(signatureSize == ED25519_SIGNATURE_LENGTH);
    APPEND_CBOR(HC_AUX_DATA, CBOR_TYPE_BYTES, signatureSize);
    APPEND_DATA(HC_AUX_DATA, signatureBuffer, signatureSize);

    builder->state = AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_SIGNATURE;
}

void auxDataHashBuilder_cVoteRegistration_addAuxiliaryScripts(aux_data_hash_builder_t* builder) {
    _TRACE("state = %d", builder->state);

    ASSERT(builder->state == AUX_DATA_HASH_BUILDER_IN_CVOTE_REGISTRATION_SIGNATURE);
    APPEND_CBOR(HC_AUX_DATA, CBOR_TYPE_ARRAY, 0);
    builder->state = AUX_DATA_HASH_BUILDER_IN_AUXILIARY_SCRIPTS;
}

void auxDataHashBuilder_finalize(aux_data_hash_builder_t* builder,
                                 uint8_t* outBuffer,
                                 size_t outSize) {
    _TRACE("state = %d", builder->state);

    ASSERT(builder->state == AUX_DATA_HASH_BUILDER_IN_AUXILIARY_SCRIPTS);
    ASSERT(outSize == AUX_DATA_HASH_LENGTH);

    blake2b_256_finalize(&builder->auxDataHash, outBuffer, outSize);
    builder->state = AUX_DATA_HASH_BUILDER_FINISHED;
}
