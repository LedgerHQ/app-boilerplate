#include "os_io_seproxyhal.h"
#include <stdint.h>

#include "keyDerivation.h"
#include "cbor.h"
#include "hash.h"
#include "base58.h"
#include "utils.h"
#include "cardano.h"
#include "securityPolicy.h"
#include "crypto.h"

static void extractRawPublicKey(uint8_t rawPubkey[static 65], uint8_t* outBuffer, size_t outSize) {
    // copy public key little endian to big endian
    ASSERT(outSize == 32);

    uint8_t i;
    for (i = 0; i < 32; i++) {
        outBuffer[i] = rawPubkey[64 - i];
    }

    if ((rawPubkey[32] & 1) != 0) {
        outBuffer[31] |= 0x80;
    }
}

// pub_key + chain_code
cx_err_t deriveExtendedPublicKey(const bip44_path_t* path, extendedPublicKey_t* out) {
    uint8_t rawPubkey[65];
    uint8_t chainCode[CHAIN_CODE_SIZE];

    STATIC_ASSERT(SIZEOF(*out) == CHAIN_CODE_SIZE + PUBLIC_KEY_SIZE, "bad ext pub key size");

    // Sanity check
    ASSERT(path->length <= ARRAY_LEN(path->path));

    // if the path is invalid, it's a bug in previous validation
    ASSERT(policyForDerivePrivateKey(path) != POLICY_DENY);

    {
        cx_err_t error = crypto_get_pubkey(path->path, path->length, rawPubkey, chainCode);
        if (error != CX_OK) {
            PRINTF("error: %d", error);
            return error;
        }
    }

    extractRawPublicKey(rawPubkey, out->pubKey, SIZEOF(out->pubKey));

    // Chain code (we copy it second to avoid mid-updates extractRawPublicKey throws
    STATIC_ASSERT(CHAIN_CODE_SIZE == SIZEOF(out->chainCode), "bad chain code size");
    STATIC_ASSERT(CHAIN_CODE_SIZE == SIZEOF(chainCode), "bad chain code size");
    memmove(out->chainCode, chainCode, CHAIN_CODE_SIZE);

    return CX_OK;
}
