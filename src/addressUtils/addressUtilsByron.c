#include "addressUtilsByron.h"
#include "keyDerivation.h"
#include "cbor.h"
#include "constants.h"
#include "hash.h"
#include "buffer_utils.h"
#include "lcx_crc.h"
#include "sw.h"

#define BYRON_ADDRESS_CBOR_HASH_SIZE 32
#define ADDRESS_ROOT_SIZE 28

static const size_t PROTOCOL_MAGIC_ADDRESS_ATTRIBUTE_KEY = 2;

enum {
    CARDANO_ADDRESS_TYPE_PUBKEY = 0,
    /*
    CARDANO_ADDRESS_TYPE_SCRIPT = 1,
    CARDANO_ADDRESS_TYPE_REDEEM = 2,
    */
};

void addressRootFromExtPubKey(const extendedPublicKey_t* extPubKey,
                              uint8_t* outBuffer,
                              size_t outSize) {
    STATIC_ASSERT(SIZEOF(*extPubKey) == EXTENDED_PUBKEY_SIZE, "wrong ext pub key size");
    ASSERT(outSize == ADDRESS_ROOT_SIZE);

    uint8_t cborBuffer[64 + 10] = {0};
    write_buffer_t cbor = buffer_init(cborBuffer, SIZEOF(cborBuffer));

    {
        // [0, [0, publicKey:chainCode], Map(0)]
        // Note(ppershing): what are the first two 0 constants?
        ASSERT(buffer_write_cbor_token(&cbor, CBOR_TYPE_ARRAY, 3));
        { ASSERT(buffer_write_cbor_token(&cbor, CBOR_TYPE_UNSIGNED, CARDANO_ADDRESS_TYPE_PUBKEY)); }
        {
            ASSERT(buffer_write_cbor_token(&cbor, CBOR_TYPE_ARRAY, 2));
            { ASSERT(buffer_write_cbor_token(&cbor, CBOR_TYPE_UNSIGNED, 0 /* this seems to be hardcoded to 0*/)); }
            {
                ASSERT(buffer_write_cbor_token(&cbor, CBOR_TYPE_BYTES, EXTENDED_PUBKEY_SIZE));
                ASSERT(buffer_write_bytes(&cbor, (const uint8_t*) extPubKey, EXTENDED_PUBKEY_SIZE));
            }
        }
        { ASSERT(buffer_write_cbor_token(&cbor, CBOR_TYPE_MAP, 0 /* addrAttributes is empty */)); }
    }

    // cborBuffer is hashed twice. First by sha3_256 and then by blake2b_224
    uint8_t cborShaHash[BYRON_ADDRESS_CBOR_HASH_SIZE] = {0};
    sha3_256_hash(cbor.ptr, buffer_written_size(&cbor), cborShaHash, SIZEOF(cborShaHash));
    blake2b_224_hash(cborShaHash, SIZEOF(cborShaHash), outBuffer, outSize);
}

size_t cborEncodePubkeyAddressInner(const uint8_t* addressRoot,
                                    size_t addressRootSize,
                                    uint32_t protocolMagic,
                                    uint8_t* outBuffer,
                                    size_t outSize
                                    /* potential attributes */
) {
    ASSERT(addressRootSize == ADDRESS_ROOT_SIZE);  // should be result of blake2b_224
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    write_buffer_t out = buffer_init(outBuffer, outSize);
    {
        // [0, [0, publicKey:chainCode], Map(0)]
        ASSERT(buffer_write_cbor_token(&out, CBOR_TYPE_ARRAY, 3));
        {
            // 1
            ASSERT(buffer_write_cbor_token(&out, CBOR_TYPE_BYTES, addressRootSize));
            ASSERT(buffer_write_bytes(&out, addressRoot, addressRootSize));
        }
        {
            // 2
            if (protocolMagic == MAINNET_PROTOCOL_MAGIC) {
                ASSERT(buffer_write_cbor_token(&out, CBOR_TYPE_MAP, 0 /* addrAttributes is empty */));
            } else {
                /* addrAttributes contains protocol magic for non-mainnet Byron addresses */
                ASSERT(buffer_write_cbor_token(&out, CBOR_TYPE_MAP, 1));
                {
                    ASSERT(buffer_write_cbor_token(
                        &out,
                        CBOR_TYPE_UNSIGNED,
                        PROTOCOL_MAGIC_ADDRESS_ATTRIBUTE_KEY)); /* map key for protocol magic */

                    // Protocol magic itself is bytes with cbor-encoded content
                    uint8_t scratch[10] = {0};
                    size_t scratchSize = cbor_writeToken(CBOR_TYPE_UNSIGNED,
                                                         protocolMagic,
                                                         scratch,
                                                         SIZEOF(scratch));
                    ASSERT(buffer_write_cbor_token(&out, CBOR_TYPE_BYTES, scratchSize));
                    ASSERT(buffer_write_bytes(&out, scratch, scratchSize));
                }
            }
        }
        {
            // 3
            ASSERT(buffer_write_cbor_token(&out, CBOR_TYPE_UNSIGNED, CARDANO_ADDRESS_TYPE_PUBKEY));
        }
    }
    return buffer_written_size(&out);
}

size_t cborPackRawAddressWithChecksum(const uint8_t* rawAddressBuffer,
                                      size_t rawAddressSize,
                                      uint8_t* outputBuffer,
                                      size_t outputSize) {
    ASSERT(rawAddressSize < BUFFER_SIZE_PARANOIA);
    ASSERT(outputSize < BUFFER_SIZE_PARANOIA);

    write_buffer_t output = buffer_init(outputBuffer, outputSize);
    {
        // Format is
        // Array[
        //     tag(24):bytes(rawAddress),
        //     crc32(rawAddress)
        // ]
        ASSERT(buffer_write_cbor_token(&output, CBOR_TYPE_ARRAY, 2));
        {
            ASSERT(buffer_write_cbor_token(&output, CBOR_TYPE_TAG, CBOR_TAG_EMBEDDED_CBOR_BYTE_STRING));
            ASSERT(buffer_write_cbor_token(&output, CBOR_TYPE_BYTES, rawAddressSize));
            ASSERT(buffer_write_bytes(&output, rawAddressBuffer, rawAddressSize));
        }
        {
            uint32_t checksum = cx_crc32(rawAddressBuffer, rawAddressSize);
            ASSERT(buffer_write_cbor_token(&output, CBOR_TYPE_UNSIGNED, checksum));
        }
    }
    return buffer_written_size(&output);
}

size_t deriveRawAddress(const bip44_path_t* pathSpec,
                        uint32_t protocolMagic,
                        uint8_t* outBuffer,
                        size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    uint8_t addressRoot[ADDRESS_ROOT_SIZE] = {0};
    {
        extendedPublicKey_t extPubKey;

        deriveExtendedPublicKey(pathSpec, &extPubKey);

        addressRootFromExtPubKey(&extPubKey, addressRoot, SIZEOF(addressRoot));
    }

    return cborEncodePubkeyAddressInner(addressRoot,
                                        SIZEOF(addressRoot),
                                        protocolMagic,
                                        outBuffer,
                                        outSize);
}

size_t deriveAddress_byron(const bip44_path_t* pathSpec,
                           uint32_t protocolMagic,
                           uint8_t* outBuffer,
                           size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    uint8_t rawAddressBuffer[40] = {0};
    size_t rawAddressSize =
        deriveRawAddress(pathSpec, protocolMagic, rawAddressBuffer, SIZEOF(rawAddressBuffer));

    return cborPackRawAddressWithChecksum(rawAddressBuffer, rawAddressSize, outBuffer, outSize);
}

// Parse helpers for extractProtocolMagic - return false on error
// These helpers work with CBOR data from address parsing.
// cbor_parseToken is now safe and never throws.
static bool parseToken(buffer_t* buf, uint8_t expectedType, uint64_t* out_value) {
    cbor_token_t token;
    size_t remaining = buf->size - buf->offset;

    // cbor_parseToken handles all bounds checking and returns false on error
    if (!cbor_parseToken(buf->ptr + buf->offset, remaining, &token)) {
        return false;
    }

    if (token.type != expectedType) {
        return false;
    }

    size_t tokenSize = token.width + 1;
    if (!buffer_seek_cur(buf, tokenSize)) {
        return false;
    }

    *out_value = token.value;
    return true;
}

static bool parseTokenWithValue(buffer_t* buf, uint8_t expectedType, uint64_t expectedValue) {
    uint64_t value;
    if (!parseToken(buf, expectedType, &value)) {
        return false;
    }
    if (value != expectedValue) {
        return false;
    }
    return true;
}

static bool parseBytesSizeToken(buffer_t* buf, size_t* out_size) {
    uint64_t parsedSize;
    if (!parseToken(buf, CBOR_TYPE_BYTES, &parsedSize)) {
        return false;
    }

    // Validate that we can down-cast
    STATIC_ASSERT(sizeof(parsedSize) >= sizeof(SIZE_MAX), "bad int size");
    if (parsedSize >= (uint64_t) SIZE_MAX) {
        return false;
    }

    size_t parsedSizeDowncasted = (size_t) parsedSize;

    // overflow pre-check
    if (parsedSizeDowncasted >= BUFFER_SIZE_PARANOIA) {
        return false;
    }

    // Check remaining size in read buffer
    size_t remaining = buf->size - buf->offset;
    if (parsedSizeDowncasted > remaining) {
        return false;
    }

    *out_size = parsedSizeDowncasted;
    return true;
}

bool extractProtocolMagic(const uint8_t* addressBuffer, size_t addressSize, uint32_t* out_protocol_magic) {
    ASSERT(addressSize < BUFFER_SIZE_PARANOIA);

    buffer_t buf = {.ptr = (uint8_t *)addressBuffer, .size = addressSize, .offset = 0};

    uint32_t protocolMagic = MAINNET_PROTOCOL_MAGIC;  // mainnet addresses do not contain protocol magic
    bool protocolMagicFound = false;

    if (!parseTokenWithValue(&buf, CBOR_TYPE_ARRAY, 2)) {
        return false;
    }

    if (!parseTokenWithValue(&buf, CBOR_TYPE_TAG, CBOR_TAG_EMBEDDED_CBOR_BYTE_STRING)) {
        return false;
    }

    size_t unboxedAddressPayloadSize;
    if (!parseBytesSizeToken(&buf, &unboxedAddressPayloadSize)) {
        return false;
    }
    const uint8_t* unboxedAddressPayload = buf.ptr + buf.offset;

    if (!parseTokenWithValue(&buf, CBOR_TYPE_ARRAY, 3)) {
        return false;
    }

    // address root (public key hash, 224 bits)
    {
        size_t parsedAddressRootSize;
        if (!parseBytesSizeToken(&buf, &parsedAddressRootSize)) {
            return false;
        }
        if (parsedAddressRootSize != ADDRESS_ROOT_SIZE) {
            return false;
        }
        if (!buffer_seek_cur(&buf, ADDRESS_ROOT_SIZE)) {
            return false;
        }
    }

    // address attributes map { key (unsigned): value(bytes) }
    {
        const size_t MAX_ADDRESS_ATTRIBUTES_MAP_LENGTH = 3;
        uint64_t addressAttributesMapLength;
        if (!parseToken(&buf, CBOR_TYPE_MAP, &addressAttributesMapLength)) {
            return false;
        }
        if (addressAttributesMapLength > (uint64_t) MAX_ADDRESS_ATTRIBUTES_MAP_LENGTH) {
            return false;
        }

        for (size_t i = 0; i < addressAttributesMapLength; i++) {
            uint64_t currentKey;
            if (!parseToken(&buf, CBOR_TYPE_UNSIGNED, &currentKey)) {
                return false;
            }

            size_t currentValueSize;
            if (!parseBytesSizeToken(&buf, &currentValueSize)) {
                return false;
            }

            if (currentKey == (uint64_t) PROTOCOL_MAGIC_ADDRESS_ATTRIBUTE_KEY) {
                if (protocolMagicFound) {
                    return false;  // duplicate protocol magic attribute
                }

                uint64_t parsedProtocolMagic;
                if (!parseToken(&buf, CBOR_TYPE_UNSIGNED, &parsedProtocolMagic)) {
                    return false;
                }

                // ensure the parsed protocol magic can be downcasted to uint32
                STATIC_ASSERT(sizeof(parsedProtocolMagic) >= sizeof(SIZE_MAX), "bad int size");
                if (parsedProtocolMagic >= (uint32_t) SIZE_MAX) {
                    return false;
                }

                protocolMagic = (uint32_t) parsedProtocolMagic;
                // mainnet addresses are not supposed to explicitly contain protocol magic at all
                if (protocolMagic == MAINNET_PROTOCOL_MAGIC) {
                    return false;
                }

                protocolMagicFound = true;
            } else {
                // skip this attribute value
                if (!buffer_seek_cur(&buf, currentValueSize)) {
                    return false;
                }
            }
        }
    }

    // address type (unsigned)
    {
        uint64_t addressType;
        if (!parseToken(&buf, CBOR_TYPE_UNSIGNED, &addressType)) {
            return false;
        }
    }

    // verify checksum
    {
        uint32_t checksum = cx_crc32(unboxedAddressPayload, unboxedAddressPayloadSize);
        if (!parseTokenWithValue(&buf, CBOR_TYPE_UNSIGNED, (uint64_t)checksum)) {
            return false;
        }
    }

    // ensure we've consumed the entire buffer
    size_t remaining = buf.size - buf.offset;
    if (remaining != 0) {
        return false;
    }

    if (!protocolMagicFound) {
        // mainnet addresses are not supposed to explicitly contain protocol magic at all
        protocolMagic = MAINNET_PROTOCOL_MAGIC;
    }

    *out_protocol_magic = protocolMagic;
    return true;
}
