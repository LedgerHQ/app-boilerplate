#include "cx.h"

#include "messageSigning.h"
#include "addressUtils/bip44.h"
#include "securityPolicy.h"
#include "crypto.h"

int signRawMessageWithPath(const bip44_path_t* path,
                            const uint8_t* messageBuffer,
                            size_t messageSize,
                            uint8_t* outBuffer,
                            size_t outSize) {
    size_t sigLen = outSize;

    ASSERT(messageSize < BUFFER_SIZE_PARANOIA);
    ASSERT(sigLen == ED25519_SIGNATURE_LENGTH);

    // Sanity check
    ASSERT(path->length <= ARRAY_LEN(path->path));

    // if the path is invalid, it's a bug in previous validation
    ASSERT(policyForDerivePrivateKey(path) != POLICY_DENY);

#if !defined(FUZZING) || defined(TEST)
    {
        TRACE("signing with path:");
        BIP44_PRINTF(path);
        TRACE("");

        cx_err_t error = crypto_eddsa_sign(path->path,
                                           path->length,
                                           messageBuffer,
                                           messageSize,
                                           outBuffer,
                                           &sigLen);
        if (error != CX_OK) {
            TRACE("error: %d", error);
            ASSERT(false);
            return error;
        }
    }
#endif

    ASSERT(sigLen == ED25519_SIGNATURE_LENGTH);
    return CX_OK;
}

// sign the given hash by the private key derived according to the given path
void getWitness(const bip44_path_t* path,
                const uint8_t* hashBuffer,
                size_t hashSize,
                uint8_t* outBuffer,
                size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

#if !defined(FUZZING) || defined(TEST)
    signRawMessageWithPath(path, hashBuffer, hashSize, outBuffer, outSize);
#endif
}

void getCVoteRegistrationSignature(const bip44_path_t* path,
                                   const uint8_t* payloadHashBuffer,
                                   size_t payloadHashSize,
                                   uint8_t* outBuffer,
                                   size_t outSize) {
    ASSERT(payloadHashSize == CVOTE_REGISTRATION_PAYLOAD_HASH_LENGTH);
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

#if !defined(FUZZING) || defined(TEST)
    signRawMessageWithPath(path, payloadHashBuffer, payloadHashSize, outBuffer, outSize);
#endif
}
