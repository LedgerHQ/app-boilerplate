#include "bufView.h"
#include "hash.h"
#include "keyDerivation.h"
#include "addressUtilsByron.h"
#include "addressUtilsShelley.h"
#include "addressUtils/bip44.h"
#include "base58.h"
#include "addressUtils/bech32.h"

uint8_t getAddressHeader(const uint8_t* addressBuffer, size_t addressSize) {
    ASSERT(addressSize > 0);
    ASSERT(addressSize < BUFFER_SIZE_PARANOIA);

    return addressBuffer[0];
}

address_type_t getAddressType(uint8_t addressHeader) {
    const uint8_t ADDRESS_TYPE_MASK = 0b11110000;
    return (addressHeader & ADDRESS_TYPE_MASK) >> 4;
}

bool isSupportedAddressType(uint8_t addressType) {
    switch (addressType) {
        case BASE_PAYMENT_KEY_STAKE_KEY:
        case BASE_PAYMENT_SCRIPT_STAKE_KEY:
        case BASE_PAYMENT_KEY_STAKE_SCRIPT:
        case BASE_PAYMENT_SCRIPT_STAKE_SCRIPT:
        case POINTER_KEY:
        case POINTER_SCRIPT:
        case ENTERPRISE_KEY:
        case ENTERPRISE_SCRIPT:
        case BYRON:
        case REWARD_KEY:
        case REWARD_SCRIPT:
            return true;
        default:
            return false;
    }
}

bool isShelleyAddressType(uint8_t addressType) {
    switch (addressType) {
        case BASE_PAYMENT_KEY_STAKE_KEY:
        case BASE_PAYMENT_SCRIPT_STAKE_KEY:
        case BASE_PAYMENT_KEY_STAKE_SCRIPT:
        case BASE_PAYMENT_SCRIPT_STAKE_SCRIPT:
        case POINTER_KEY:
        case POINTER_SCRIPT:
        case ENTERPRISE_KEY:
        case ENTERPRISE_SCRIPT:
        case REWARD_KEY:
        case REWARD_SCRIPT:
            return true;
        default:
            return false;
    }
}

uint8_t constructShelleyAddressHeader(address_type_t type, uint8_t networkId) {
    ASSERT(isSupportedAddressType(type));
    ASSERT(isValidNetworkId(networkId));

    return (type << 4) | networkId;
}

uint8_t getNetworkId(uint8_t addressHeader) {
    const uint8_t NETWORK_ID_MASK = 0b00001111;
    return addressHeader & NETWORK_ID_MASK;
}

bool isValidNetworkId(uint8_t networkId) {
    return networkId <= MAXIMUM_NETWORK_ID;
}

bool isValidStakingChoice(staking_data_source_t stakingDataSource) {
    switch (stakingDataSource) {
        case NO_STAKING:
        case STAKING_KEY_PATH:
        case STAKING_KEY_HASH:
        case BLOCKCHAIN_POINTER:
        case STAKING_SCRIPT_HASH:
            return true;
        default:
            return false;
    }
}

bool isStakingInfoConsistentWithAddressType(const addressParams_t* addressParams) {
#define CONSISTENT_WITH(STAKING_CHOICE) \
    if (addressParams->stakingDataSource == (STAKING_CHOICE)) return true

    switch (addressParams->type) {
        case BASE_PAYMENT_KEY_STAKE_KEY:
        case BASE_PAYMENT_SCRIPT_STAKE_KEY:
        case REWARD_KEY:
            CONSISTENT_WITH(STAKING_KEY_HASH);
            CONSISTENT_WITH(STAKING_KEY_PATH);
            break;

        case BASE_PAYMENT_KEY_STAKE_SCRIPT:
        case BASE_PAYMENT_SCRIPT_STAKE_SCRIPT:
        case REWARD_SCRIPT:
            CONSISTENT_WITH(STAKING_SCRIPT_HASH);

            __attribute__((fallthrough));
        case POINTER_KEY:
        case POINTER_SCRIPT:
            CONSISTENT_WITH(BLOCKCHAIN_POINTER);
            break;

        case ENTERPRISE_KEY:
        case ENTERPRISE_SCRIPT:
        case BYRON:
            CONSISTENT_WITH(NO_STAKING);
            break;

        default:
            ASSERT(false);
    }

    return false;

#undef CONSISTENT_WITH
}

staking_data_source_t determineStakingChoice(address_type_t addressType) {
    switch (addressType) {
        case BASE_PAYMENT_KEY_STAKE_KEY:
        case BASE_PAYMENT_SCRIPT_STAKE_KEY:
        case REWARD_KEY:
            return STAKING_KEY_HASH;

        case BASE_PAYMENT_KEY_STAKE_SCRIPT:
        case BASE_PAYMENT_SCRIPT_STAKE_SCRIPT:
        case REWARD_SCRIPT:
            return STAKING_SCRIPT_HASH;

        case POINTER_KEY:
        case POINTER_SCRIPT:
            return BLOCKCHAIN_POINTER;

        case ENTERPRISE_KEY:
        case ENTERPRISE_SCRIPT:
        case BYRON:
            return NO_STAKING;

        default:
            ASSERT(false);
    }
}

__noinline_due_to_stack__ static size_t view_appendAddressPublicKeyHash(
    write_view_t* view,
    const bip44_path_t* keyDerivationPath) {

    uint8_t hashedPubKey[ADDRESS_KEY_HASH_LENGTH] = {0};
    bip44_pathToKeyHash(keyDerivationPath, hashedPubKey, SIZEOF(hashedPubKey));

    view_appendBuffer(view, hashedPubKey, SIZEOF(hashedPubKey));

    return ADDRESS_KEY_HASH_LENGTH;
}

static bool _isBaseAddress(address_type_t addressType) {
    switch (addressType) {
        case BASE_PAYMENT_KEY_STAKE_KEY:
        case BASE_PAYMENT_SCRIPT_STAKE_KEY:
        case BASE_PAYMENT_KEY_STAKE_SCRIPT:
        case BASE_PAYMENT_SCRIPT_STAKE_SCRIPT:
            return true;
        default:
            return false;
    }
}

static size_t deriveAddress_base(const addressParams_t* addressParams,
                                 uint8_t* outBuffer,
                                 size_t outSize) {
    ASSERT(_isBaseAddress(addressParams->type));
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    const uint8_t header =
        constructShelleyAddressHeader(addressParams->type, addressParams->networkId);
    write_view_t out = make_write_view(outBuffer, outBuffer + outSize);

    size_t size = 0;
    {
        view_appendBuffer(&out, &header, 1);
        ++size;
    }
    STATIC_ASSERT(SIZEOF(addressParams->paymentScriptHash) == SCRIPT_HASH_LENGTH,
                  "bad payment script hash size");
    switch (addressParams->type) {
        case BASE_PAYMENT_KEY_STAKE_KEY:
        case BASE_PAYMENT_KEY_STAKE_SCRIPT: {
            view_appendAddressPublicKeyHash(&out, &addressParams->paymentKeyPath);
            size += ADDRESS_KEY_HASH_LENGTH;
        } break;
        case BASE_PAYMENT_SCRIPT_STAKE_KEY:
        case BASE_PAYMENT_SCRIPT_STAKE_SCRIPT: {
            view_appendBuffer(&out, addressParams->paymentScriptHash, SCRIPT_HASH_LENGTH);
            size += SCRIPT_HASH_LENGTH;
        } break;
        default:
            ASSERT(false);
    }

    STATIC_ASSERT(SIZEOF(addressParams->stakingKeyHash) == ADDRESS_KEY_HASH_LENGTH,
                  "bad stake key hash size");
    STATIC_ASSERT(SIZEOF(addressParams->stakingScriptHash) == SCRIPT_HASH_LENGTH,
                  "bad stake script hash size");
    switch (addressParams->stakingDataSource) {
        case STAKING_KEY_PATH: {
            view_appendAddressPublicKeyHash(&out, &addressParams->stakingKeyPath);
            size += ADDRESS_KEY_HASH_LENGTH;
        } break;

        case STAKING_KEY_HASH: {
            view_appendBuffer(&out, addressParams->stakingKeyHash, ADDRESS_KEY_HASH_LENGTH);
            size += ADDRESS_KEY_HASH_LENGTH;
        } break;

        case STAKING_SCRIPT_HASH: {
            view_appendBuffer(&out, addressParams->stakingScriptHash, SCRIPT_HASH_LENGTH);
            size += SCRIPT_HASH_LENGTH;
        } break;
        default:
            ASSERT(false);
    }
    ASSERT(view_processedSize(&out) == size);
    return size;
}

static size_t view_appendVariableLengthUInt(write_view_t* view, uint64_t value) {
    ASSERT(value < (1llu << 63));  // avoid accidental cast from negative signed value

    if (value == 0) {
        uint8_t byte = 0;
        view_appendBuffer(view, &byte, 1);
        return 1;
    }

    ASSERT(value > 0);

    uint8_t chunks[10] = {0};  // 7-bit chunks of the input bits, at most 10 in uint64
    size_t outputSize = 0;
    {
        blockchainIndex_t bits = value;
        while (bits > 0) {
            // take next 7 bits from the right
            chunks[outputSize++] = bits & 0b01111111;
            bits >>= 7;
        }
    }
    ASSERT(outputSize > 0);
    for (size_t i = outputSize - 1; i > 0; --i) {
        // highest bit set to 1 since more bytes follow
        uint8_t nextByte = chunks[i] | 0b10000000;
        view_appendBuffer(view, &nextByte, 1);
    }
    // write the remaining byte, highest bit 0
    view_appendBuffer(view, &chunks[0], 1);

    return outputSize;
}

static size_t deriveAddress_pointer(const addressParams_t* addressParams,
                                    uint8_t* outBuffer,
                                    size_t outSize) {
    const address_type_t addressType = addressParams->type;
    ASSERT(addressType == POINTER_KEY || addressType == POINTER_SCRIPT);
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    const uint8_t addressHeader =
        constructShelleyAddressHeader(addressType, addressParams->networkId);

    write_view_t out = make_write_view(outBuffer, outBuffer + outSize);
    { view_appendBuffer(&out, &addressHeader, 1); }
    {
        if (addressType == POINTER_KEY) {
            view_appendAddressPublicKeyHash(&out, &addressParams->paymentKeyPath);
        } else {
            view_appendBuffer(&out, addressParams->paymentScriptHash, SCRIPT_HASH_LENGTH);
        }

        STATIC_ASSERT(SCRIPT_HASH_LENGTH == ADDRESS_KEY_HASH_LENGTH, "incompatible hash lengths");
        const int ADDRESS_LENGTH = 1 + ADDRESS_KEY_HASH_LENGTH;
        ASSERT(view_processedSize(&out) == ADDRESS_LENGTH);
    }
    {
        const blockchainPointer_t* stakingKeyBlockchainPointer =
            &addressParams->stakingKeyBlockchainPointer;
        view_appendVariableLengthUInt(&out, stakingKeyBlockchainPointer->blockIndex);
        view_appendVariableLengthUInt(&out, stakingKeyBlockchainPointer->txIndex);
        view_appendVariableLengthUInt(&out, stakingKeyBlockchainPointer->certificateIndex);
    }

    return view_processedSize(&out);
}

static size_t deriveAddress_enterprise(const addressParams_t* addressParams,
                                       uint8_t* outBuffer,
                                       size_t outSize) {
    const address_type_t addressType = addressParams->type;
    ASSERT(addressType == ENTERPRISE_KEY || addressType == ENTERPRISE_SCRIPT);
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    const uint8_t addressHeader =
        constructShelleyAddressHeader(addressType, addressParams->networkId);

    write_view_t out = make_write_view(outBuffer, outBuffer + outSize);
    { view_appendBuffer(&out, &addressHeader, 1); }
    {
        if (addressType == ENTERPRISE_KEY) {
            view_appendAddressPublicKeyHash(&out, &addressParams->paymentKeyPath);
        } else {
            view_appendBuffer(&out, addressParams->paymentScriptHash, SCRIPT_HASH_LENGTH);
        }
    }
    {
        // no staking data
    }

    STATIC_ASSERT(SCRIPT_HASH_LENGTH == ADDRESS_KEY_HASH_LENGTH, "incompatible hash lengths");
    const int ADDRESS_LENGTH = 1 + ADDRESS_KEY_HASH_LENGTH;
    ASSERT(view_processedSize(&out) == ADDRESS_LENGTH);

    return ADDRESS_LENGTH;
}

static size_t deriveAddress_reward(const addressParams_t* addressParams,
                                   uint8_t* outBuffer,
                                   size_t outSize) {
    const address_type_t addressType = addressParams->type;
    ASSERT(addressType == REWARD_KEY || addressType == REWARD_SCRIPT);
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    const uint8_t addressHeader =
        constructShelleyAddressHeader(addressType, addressParams->networkId);

    write_view_t out = make_write_view(outBuffer, outBuffer + outSize);
    { view_appendBuffer(&out, &addressHeader, 1); }
    {
        // no payment data
    } {
        if (addressType == REWARD_KEY) {
            const bip44_path_t* stakingKeyPath = &addressParams->stakingKeyPath;
            // stake key path expected (corresponds to reward account)
            BIP44_PRINTF(stakingKeyPath);
            PRINTF("\n");
            ASSERT(bip44_isOrdinaryStakingKeyPath(stakingKeyPath));
            view_appendAddressPublicKeyHash(&out, stakingKeyPath);
        } else {
            view_appendBuffer(&out, addressParams->stakingScriptHash, SCRIPT_HASH_LENGTH);
        }
    }

    STATIC_ASSERT(SCRIPT_HASH_LENGTH == ADDRESS_KEY_HASH_LENGTH, "incompatible hash lengths");
    const int ADDRESS_LENGTH = 1 + ADDRESS_KEY_HASH_LENGTH;
    ASSERT(view_processedSize(&out) == ADDRESS_LENGTH);

    return ADDRESS_LENGTH;
}

size_t constructRewardAddressFromKeyPath(const bip44_path_t* path,
                                         uint8_t networkId,
                                         uint8_t* outBuffer,
                                         size_t outSize) {
    ASSERT(outSize == REWARD_ACCOUNT_SIZE);
    ASSERT(bip44_isOrdinaryStakingKeyPath(path));

    addressParams_t addressParamsStub;
    addressParamsStub.type = REWARD_KEY;
    addressParamsStub.networkId = networkId;
    addressParamsStub.stakingDataSource = STAKING_KEY_HASH;
    addressParamsStub.stakingKeyPath = *path;
    return deriveAddress_reward(&addressParamsStub, outBuffer, outSize);
}

size_t constructRewardAddressFromHash(uint8_t networkId,
                                      reward_address_hash_source_t source,
                                      const uint8_t* hashBuffer,
                                      size_t hashSize,
                                      uint8_t* outBuffer,
                                      size_t outSize) {
    ASSERT(isValidNetworkId(networkId));
    ASSERT(hashSize == ADDRESS_KEY_HASH_LENGTH);
    STATIC_ASSERT(ADDRESS_KEY_HASH_LENGTH == SCRIPT_HASH_LENGTH, "incompatible hash sizes");
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    write_view_t out = make_write_view(outBuffer, outBuffer + outSize);
    {
        const uint8_t addressHeader = constructShelleyAddressHeader(
            (source == REWARD_HASH_SOURCE_KEY) ? REWARD_KEY : REWARD_SCRIPT,
            networkId);
        view_appendBuffer(&out, &addressHeader, 1);
    }
    { view_appendBuffer(&out, hashBuffer, hashSize); }

    const int ADDRESS_LENGTH = REWARD_ACCOUNT_SIZE;
    ASSERT(view_processedSize(&out) == ADDRESS_LENGTH);

    return ADDRESS_LENGTH;
}

size_t deriveAddress(const addressParams_t* addressParams, uint8_t* outBuffer, size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);
    ASSERT(isValidAddressParams(addressParams));

    // shelley
    switch (addressParams->type) {
        case BASE_PAYMENT_KEY_STAKE_KEY:
        case BASE_PAYMENT_SCRIPT_STAKE_KEY:
        case BASE_PAYMENT_KEY_STAKE_SCRIPT:
        case BASE_PAYMENT_SCRIPT_STAKE_SCRIPT:
            return deriveAddress_base(addressParams, outBuffer, outSize);
        case POINTER_KEY:
        case POINTER_SCRIPT:
            ASSERT(addressParams->stakingDataSource == BLOCKCHAIN_POINTER);
            return deriveAddress_pointer(addressParams, outBuffer, outSize);
        case ENTERPRISE_KEY:
        case ENTERPRISE_SCRIPT:
            return deriveAddress_enterprise(addressParams, outBuffer, outSize);
        case REWARD_KEY:
        case REWARD_SCRIPT:
            return deriveAddress_reward(addressParams, outBuffer, outSize);

        case BYRON:
            return deriveAddress_byron(&addressParams->paymentKeyPath,
                                       addressParams->protocolMagic,
                                       outBuffer,
                                       outSize);

        default:
            ASSERT(false);
    }
    return BUFFER_SIZE_PARANOIA + 1;
}

void printBlockchainPointerToStr(blockchainPointer_t blockchainPointer, char* out, size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    STATIC_ASSERT(sizeof(blockchainIndex_t) <= sizeof(unsigned), "oversized type for %u");
    STATIC_ASSERT(!IS_SIGNED(blockchainPointer.blockIndex), "signed type for %u");
    STATIC_ASSERT(!IS_SIGNED(blockchainPointer.txIndex), "signed type for %u");
    STATIC_ASSERT(!IS_SIGNED(blockchainPointer.certificateIndex), "signed type for %u");

    ASSERT(outSize > 0);
    snprintf(out,
             outSize,
             "(%u, %u, %u)",
             blockchainPointer.blockIndex,
             blockchainPointer.txIndex,
             blockchainPointer.certificateIndex);
    // make sure all the information is displayed to the user
    ASSERT(strlen(out) + 1 < outSize);
}

// bech32 for Shelley, base58 for Byron
size_t humanReadableAddress(const uint8_t* address, size_t addressSize, char* out, size_t outSize) {
    ASSERT(addressSize > 0);
    ASSERT(addressSize < BUFFER_SIZE_PARANOIA);
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    const uint8_t addressType = getAddressType(address[0]);
    const uint8_t networkId = getNetworkId(address[0]);

    if (addressType == BYRON) {
        return base58_encode(address, addressSize, out, outSize);
    }

    ASSERT(isValidNetworkId(networkId));

    switch (addressType) {
        case BYRON:
            ASSERT(false);

            __attribute__((fallthrough));
        case REWARD_KEY:
        case REWARD_SCRIPT:
            if (networkId == TESTNET_NETWORK_ID)
                return bech32_encode("stake_test", address, addressSize, out, outSize);
            else
                return bech32_encode("stake", address, addressSize, out, outSize);

        default:  // all other shelley addresses
            if (networkId == TESTNET_NETWORK_ID)
                return bech32_encode("addr_test", address, addressSize, out, outSize);
            else
                return bech32_encode("addr", address, addressSize, out, outSize);
    }
}

/*
 * Apart from parsing, we validate that the input contains nothing more than the params.
 *
 * The serialization format:
 *
 * address type 1B
 * if address type == BYRON
 *     protocol magic 4B
 * else
 *     network id 1B
 * payment public key derivation path (1B for length + [0-10] x 4B)
 * staking choice 1B
 *     if NO_STAKING:
 *         nothing more
 *     if STAKING_KEY_PATH:
 *         staking public key derivation path (1B for length + [0-10] x 4B)
 *     if STAKING_KEY_HASH:
 *         stake key hash 28B
 *     if BLOCKCHAIN_POINTER:
 *         certificate blockchain pointer 3 x 4B
 *
 * (see also enums in addressUtilsShelley.h)
 */
void view_parseAddressParams(read_view_t* view, addressParams_t* params) {
    // address type
    params->type = parse_u1be(view);
    TRACE("Address type: 0x%x", params->type);
    VALIDATE(isSupportedAddressType(params->type), ERR_INVALID_DATA);

    // protocol magic / network id
    if (params->type == BYRON) {
        params->protocolMagic = parse_u4be(view);
        TRACE("Protocol magic: 0x%x", params->protocolMagic);
    } else {
        params->networkId = parse_u1be(view);
        TRACE("Network id: 0x%x", params->networkId);
        VALIDATE(isValidNetworkId(params->networkId), ERR_INVALID_DATA);
    }
    // payment part
    switch (params->type) {
        case BASE_PAYMENT_KEY_STAKE_KEY:
        case BASE_PAYMENT_KEY_STAKE_SCRIPT:
        case POINTER_KEY:
        case ENTERPRISE_KEY:
        case BYRON:
            view_skipBytes(view,
                           bip44_parseFromWire(&params->paymentKeyPath,
                                               VIEW_REMAINING_TO_TUPLE_BUF_SIZE(view)));
            BIP44_PRINTF(&params->paymentKeyPath);
            PRINTF("\n");
            break;

        case BASE_PAYMENT_SCRIPT_STAKE_KEY:
        case BASE_PAYMENT_SCRIPT_STAKE_SCRIPT:
        case POINTER_SCRIPT:
        case ENTERPRISE_SCRIPT: {
            STATIC_ASSERT(SIZEOF(params->paymentScriptHash) == SCRIPT_HASH_LENGTH,
                          "Wrong address key hash length");
            view_parseBuffer(params->paymentScriptHash, view, SCRIPT_HASH_LENGTH);
            TRACE("Payment script hash: ");
            TRACE_BUFFER(params->paymentScriptHash, SIZEOF(params->paymentScriptHash));
            break;
        }

        case REWARD_KEY:
        case REWARD_SCRIPT:
            // no payment info for reward address types
            break;

        default:
            ASSERT(false);
            break;
    }

    // staking choice
    params->stakingDataSource = parse_u1be(view);
    TRACE("Staking choice: 0x%x", (unsigned int) params->stakingDataSource);
    VALIDATE(isValidStakingChoice(params->stakingDataSource), ERR_INVALID_DATA);

    // staking choice determines what to parse next
    switch (params->stakingDataSource) {
        case NO_STAKING:
            break;

        case STAKING_KEY_PATH:
            view_skipBytes(view,
                           bip44_parseFromWire(&params->stakingKeyPath,
                                               VIEW_REMAINING_TO_TUPLE_BUF_SIZE(view)));
            BIP44_PRINTF(&params->stakingKeyPath);
            PRINTF("\n");
            break;

        case STAKING_KEY_HASH: {
            STATIC_ASSERT(SIZEOF(params->stakingKeyHash) == ADDRESS_KEY_HASH_LENGTH,
                          "Wrong address key hash length");
            view_parseBuffer(params->stakingKeyHash, view, ADDRESS_KEY_HASH_LENGTH);
            TRACE("Stake key hash: ");
            TRACE_BUFFER(params->stakingKeyHash, SIZEOF(params->stakingKeyHash));
            break;
        }

        case STAKING_SCRIPT_HASH: {
            STATIC_ASSERT(SIZEOF(params->stakingScriptHash) == SCRIPT_HASH_LENGTH,
                          "Wrong script hash length");
            view_parseBuffer(params->stakingScriptHash, view, SCRIPT_HASH_LENGTH);
            TRACE("Stake script hash: ");
            TRACE_BUFFER(params->stakingScriptHash, SIZEOF(params->stakingScriptHash));
            break;
        }

        case BLOCKCHAIN_POINTER:
            params->stakingKeyBlockchainPointer.blockIndex = parse_u4be(view);
            params->stakingKeyBlockchainPointer.txIndex = parse_u4be(view);
            params->stakingKeyBlockchainPointer.certificateIndex = parse_u4be(view);
            TRACE("Stake key pointer: [%d, %d, %d]",
                  params->stakingKeyBlockchainPointer.blockIndex,
                  params->stakingKeyBlockchainPointer.txIndex,
                  params->stakingKeyBlockchainPointer.certificateIndex);
            break;

        default:
            ASSERT(false);
    }
}

static inline bool isValidStakingInfo(const addressParams_t* params) {
#define CHECK(cond) \
    if (!(cond)) return false
    CHECK(isStakingInfoConsistentWithAddressType(params));
    if (params->stakingDataSource == STAKING_KEY_PATH) {
        CHECK(bip44_classifyPath(&params->stakingKeyPath) == PATH_ORDINARY_STAKING_KEY);
    }
    return true;
#undef CHECK
}

static inline bool isValidPaymentInfo(const addressParams_t* params) {
#define CHECK(cond) \
    if (!(cond)) return false
    switch (params->type) {
        case BYRON:
            CHECK(bip44_classifyPath(&params->paymentKeyPath) == PATH_ORDINARY_PAYMENT_KEY);
            CHECK(bip44_hasByronPrefix(&params->paymentKeyPath));
            break;

        case BASE_PAYMENT_KEY_STAKE_KEY:
        case BASE_PAYMENT_KEY_STAKE_SCRIPT:
        case POINTER_KEY:
        case ENTERPRISE_KEY:
            CHECK(bip44_classifyPath(&params->paymentKeyPath) == PATH_ORDINARY_PAYMENT_KEY);
            CHECK(bip44_hasShelleyPrefix(&params->paymentKeyPath));
            break;

        case BASE_PAYMENT_SCRIPT_STAKE_KEY:
        case BASE_PAYMENT_SCRIPT_STAKE_SCRIPT:
        case POINTER_SCRIPT:
        case ENTERPRISE_SCRIPT:
        case REWARD_KEY:
        case REWARD_SCRIPT:
            // nothing to validate
            break;

        default:
            ASSERT(false);
            break;
    }
    return true;
#undef CHECK
}

bool isValidAddressParams(const addressParams_t* params) {
#define CHECK(cond) \
    if (!(cond)) return false
    if (params->type != BYRON) {
        CHECK(isValidNetworkId(params->networkId));
    } else {
        return false;
    }

    CHECK(isValidStakingInfo(params));
    CHECK(isValidPaymentInfo(params));

    return true;
#undef CHECK
}

payment_choice_t determinePaymentChoice(address_type_t addressType) {
    switch (addressType) {
        case BASE_PAYMENT_KEY_STAKE_KEY:
        case BASE_PAYMENT_KEY_STAKE_SCRIPT:
        case POINTER_KEY:
        case ENTERPRISE_KEY:
        case BYRON:
            return PAYMENT_PATH;

        case BASE_PAYMENT_SCRIPT_STAKE_KEY:
        case BASE_PAYMENT_SCRIPT_STAKE_SCRIPT:
        case POINTER_SCRIPT:
        case ENTERPRISE_SCRIPT:
            return PAYMENT_SCRIPT_HASH;

        default:
            ASSERT(false);
            __attribute__((fallthrough));
        case REWARD_KEY:
        case REWARD_SCRIPT:
            return PAYMENT_NONE;
    }
}
