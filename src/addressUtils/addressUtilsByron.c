#include "addressUtilsByron.h"
#include "keyDerivation.h"
#include "cbor.h"
#include "cardano.h"
#include "hash.h"
#include "bufView.h"
#include "lcx_crc.h"
#include "sw.h"

static const size_t ADDRESS_ROOT_SIZE = 28;
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
    write_view_t cbor = make_write_view(cborBuffer, cborBuffer + SIZEOF(cborBuffer));

    {
        // [0, [0, publicKey:chainCode], Map(0)]
        // Note(ppershing): what are the first two 0 constants?
        view_appendToken(&cbor, CBOR_TYPE_ARRAY, 3);
        { view_appendToken(&cbor, CBOR_TYPE_UNSIGNED, CARDANO_ADDRESS_TYPE_PUBKEY); }
        {
            view_appendToken(&cbor, CBOR_TYPE_ARRAY, 2);
            { view_appendToken(&cbor, CBOR_TYPE_UNSIGNED, 0 /* this seems to be hardcoded to 0*/); }
            {
                view_appendToken(&cbor, CBOR_TYPE_BYTES, EXTENDED_PUBKEY_SIZE);
                view_appendBuffer(&cbor, (const uint8_t*) extPubKey, EXTENDED_PUBKEY_SIZE);
            }
        }
        { view_appendToken(&cbor, CBOR_TYPE_MAP, 0 /* addrAttributes is empty */); }
    }

    // cborBuffer is hashed twice. First by sha3_256 and then by blake2b_224
    uint8_t cborShaHash[32] = {0};
    sha3_256_hash(VIEW_PROCESSED_TO_TUPLE_BUF_SIZE(&cbor), cborShaHash, SIZEOF(cborShaHash));
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

    write_view_t out = make_write_view(outBuffer, outBuffer + outSize);
    {
        view_appendToken(&out, CBOR_TYPE_ARRAY, 3);
        {
            // 1
            view_appendToken(&out, CBOR_TYPE_BYTES, addressRootSize);
            view_appendBuffer(&out, addressRoot, addressRootSize);
        }
        {
            // 2
            if (protocolMagic == MAINNET_PROTOCOL_MAGIC) {
                view_appendToken(&out, CBOR_TYPE_MAP, 0 /* addrAttributes is empty */);
            } else {
                /* addrAttributes contains protocol magic for non-mainnet Byron addresses */
                view_appendToken(&out, CBOR_TYPE_MAP, 1);
                {
                    view_appendToken(
                        &out,
                        CBOR_TYPE_UNSIGNED,
                        PROTOCOL_MAGIC_ADDRESS_ATTRIBUTE_KEY); /* map key for protocol magic */

                    // Protocol magic itself is bytes with cbor-encoded content
                    uint8_t scratch[10] = {0};
                    size_t scratchSize = cbor_writeToken(CBOR_TYPE_UNSIGNED,
                                                         protocolMagic,
                                                         scratch,
                                                         SIZEOF(scratch));
                    view_appendToken(&out, CBOR_TYPE_BYTES, scratchSize);
                    view_appendBuffer(&out, scratch, scratchSize);
                }
            }
        }
        {
            // 3
            view_appendToken(&out, CBOR_TYPE_UNSIGNED, CARDANO_ADDRESS_TYPE_PUBKEY);
        }
    }
    return view_processedSize(&out);
}

size_t cborPackRawAddressWithChecksum(const uint8_t* rawAddressBuffer,
                                      size_t rawAddressSize,
                                      uint8_t* outputBuffer,
                                      size_t outputSize) {
    ASSERT(rawAddressSize < BUFFER_SIZE_PARANOIA);
    ASSERT(outputSize < BUFFER_SIZE_PARANOIA);

    write_view_t output = make_write_view(outputBuffer, outputBuffer + outputSize);

    {
        // Format is
        // Array[
        //     tag(24):bytes(rawAddress),
        //     crc32(rawAddress)
        // ]
        view_appendToken(&output, CBOR_TYPE_ARRAY, 2);
        {
            view_appendToken(&output, CBOR_TYPE_TAG, CBOR_TAG_EMBEDDED_CBOR_BYTE_STRING);
            view_appendToken(&output, CBOR_TYPE_BYTES, rawAddressSize);
            view_appendBuffer(&output, rawAddressBuffer, rawAddressSize);
        }
        {
            uint32_t checksum = cx_crc32(rawAddressBuffer, rawAddressSize);
            view_appendToken(&output, CBOR_TYPE_UNSIGNED, checksum);
        }
    }
    return view_processedSize(&output);
}

size_t deriveRawAddress(const bip44_path_t* pathSpec,
                        uint32_t protocolMagic,
                        uint8_t* outBuffer,
                        size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    uint8_t addressRoot[28] = {0};
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

static uint64_t parseToken(read_view_t* view, uint8_t type) {
    const cbor_token_t token = view_parseToken(view);
    VALIDATE(token.type == type, ERR_INVALID_DATA);
    return token.value;
}

static void parseTokenWithValue(read_view_t* view, uint8_t type, uint64_t value) {
    const cbor_token_t token = view_parseToken(view);
    VALIDATE(token.type == type, ERR_INVALID_DATA);
    VALIDATE(token.value == value, ERR_INVALID_DATA);
}

static size_t parseBytesSizeToken(read_view_t* view) {
    uint64_t parsedSize = parseToken(view, CBOR_TYPE_BYTES);
    // Validate that we can down-cast
    STATIC_ASSERT(sizeof(parsedSize) >= sizeof(SIZE_MAX), "bad int size");
    VALIDATE(parsedSize < (uint64_t) SIZE_MAX, ERR_INVALID_DATA);

    size_t parsedSizeDowncasted = (size_t) parsedSize;

    // overflow pre-check
    VALIDATE(parsedSizeDowncasted < BUFFER_SIZE_PARANOIA, ERR_INVALID_DATA);
    VALIDATE(parsedSizeDowncasted <= view_remainingSize(view), ERR_INVALID_DATA);

    return parsedSizeDowncasted;
}

uint32_t extractProtocolMagic(const uint8_t* addressBuffer, size_t addressSize) {
    ASSERT(addressSize < BUFFER_SIZE_PARANOIA);

    read_view_t view = make_read_view(addressBuffer, addressBuffer + addressSize);

    uint32_t protocolMagic =
        MAINNET_PROTOCOL_MAGIC;  // mainnet addresses do not contain protocol magic
    bool protocolMagicFound = false;
    {
        const uint8_t* unboxedAddressPayload;
        size_t unboxedAddressPayloadSize;
        parseTokenWithValue(&view, CBOR_TYPE_ARRAY, 2);
        {
            parseTokenWithValue(&view, CBOR_TYPE_TAG, CBOR_TAG_EMBEDDED_CBOR_BYTE_STRING);

            unboxedAddressPayloadSize = parseBytesSizeToken(&view);
            unboxedAddressPayload = view.ptr;

            parseTokenWithValue(&view, CBOR_TYPE_ARRAY, 3);
            {
                // address root (public key hash, 224 bits)
                {
                    size_t parsedAddressRootSize = parseBytesSizeToken(&view);
                    VALIDATE(parsedAddressRootSize == ADDRESS_ROOT_SIZE, ERR_INVALID_DATA);
                    view_skipBytes(&view, ADDRESS_ROOT_SIZE);
                }

                // address attributes map { key (unsigned): value(bytes) }
                {
                    /*
                     * max attributes threshold based on
                     * https://github.com/input-output-hk/cardano-wallet/wiki/About-Address-Format---Byron
                     * address attributes are technically "open for extension" but unlikely to
                     * happen as byron addresses are already being deprecated
                     */
                    const size_t MAX_ADDRESS_ATTRIBUTES_MAP_LENGTH = 3;
                    uint64_t addressAttributesMapLength = parseToken(&view, CBOR_TYPE_MAP);
                    VALIDATE(
                        addressAttributesMapLength <= (uint64_t) MAX_ADDRESS_ATTRIBUTES_MAP_LENGTH,
                        ERR_INVALID_DATA);

                    for (size_t i = 0; i < addressAttributesMapLength; i++) {
                        uint64_t currentKey = parseToken(&view, CBOR_TYPE_UNSIGNED);
                        size_t currentValueSize = parseBytesSizeToken(&view);

                        if (currentKey == (uint64_t) PROTOCOL_MAGIC_ADDRESS_ATTRIBUTE_KEY) {
                            VALIDATE(protocolMagicFound == false, ERR_INVALID_DATA);

                            uint64_t parsedProtocolMagic = parseToken(&view, CBOR_TYPE_UNSIGNED);
                            // ensure the parsed protocol magic can be downcasted to uint32 (its
                            // size by spec)
                            STATIC_ASSERT(sizeof(parsedProtocolMagic) >= sizeof(SIZE_MAX),
                                          "bad int size");
                            VALIDATE(parsedProtocolMagic < (uint32_t) SIZE_MAX, ERR_INVALID_DATA);

                            protocolMagic = (uint32_t) parsedProtocolMagic;
                            // mainnet addresses are not supposed to explicitly contain protocol
                            // magic at all
                            VALIDATE(protocolMagic != MAINNET_PROTOCOL_MAGIC, ERR_INVALID_DATA);

                            protocolMagicFound = true;
                        } else {
                            view_skipBytes(&view, currentValueSize);
                        }
                    }
                }

                // address type (unsigned)
                { parseToken(&view, CBOR_TYPE_UNSIGNED); }
            }
        }
        {
            uint32_t checksum = cx_crc32(unboxedAddressPayload, unboxedAddressPayloadSize);
            parseTokenWithValue(&view, CBOR_TYPE_UNSIGNED, checksum);
        }
    }

    VALIDATE(view_remainingSize(&view) == 0, ERR_INVALID_DATA);

    if (!protocolMagicFound) {
        // mainnet addresses are not supposed to explicitly contain protocol magic at all
        protocolMagic = MAINNET_PROTOCOL_MAGIC;
    }

    return protocolMagic;
}
