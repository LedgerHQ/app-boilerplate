#include "buffer_utils.h"
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
    const uint8_t ADDRESS_TYPE_MASK = 0xF0;
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
    const uint8_t NETWORK_ID_MASK = 0x0F;
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

__noinline_due_to_stack__ static bool buffer_appendAddressPublicKeyHash(
    write_buffer_t* buf,
    const bip44_path_t* keyDerivationPath) {

    uint8_t hashedPubKey[ADDRESS_KEY_HASH_LENGTH] = {0};
    bip44_pathToKeyHash(keyDerivationPath, hashedPubKey, SIZEOF(hashedPubKey));

    return buffer_write_bytes(buf, hashedPubKey, SIZEOF(hashedPubKey));
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
    write_buffer_t out = buffer_init(outBuffer, outSize);
    {
        ASSERT(buffer_write_bytes(&out, &header, 1));
    }
    STATIC_ASSERT(SIZEOF(addressParams->paymentScriptHash) == SCRIPT_HASH_LENGTH,
                  "bad payment script hash size");
    switch (addressParams->type) {
        case BASE_PAYMENT_KEY_STAKE_KEY:
        case BASE_PAYMENT_KEY_STAKE_SCRIPT: {
            ASSERT(buffer_appendAddressPublicKeyHash(&out, &addressParams->paymentKeyPath));
        } break;
        case BASE_PAYMENT_SCRIPT_STAKE_KEY:
        case BASE_PAYMENT_SCRIPT_STAKE_SCRIPT: {
            ASSERT(buffer_write_bytes(&out, addressParams->paymentScriptHash, SCRIPT_HASH_LENGTH));
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
            ASSERT(buffer_appendAddressPublicKeyHash(&out, &addressParams->stakingKeyPath));
        } break;

        case STAKING_KEY_HASH: {
            ASSERT(buffer_write_bytes(&out, addressParams->stakingKeyHash, ADDRESS_KEY_HASH_LENGTH));
        } break;

        case STAKING_SCRIPT_HASH: {
            ASSERT(buffer_write_bytes(&out, addressParams->stakingScriptHash, SCRIPT_HASH_LENGTH));
        } break;
        default:
            ASSERT(false);
    }
    return buffer_written_size(&out);
}

static bool buffer_appendVariableLengthUInt(write_buffer_t* buf, uint64_t value) {
    ASSERT(value < (1llu << 63));  // avoid accidental cast from negative signed value

    if (value == 0) {
        uint8_t byte = 0;
        return buffer_write_bytes(buf, &byte, 1);
    }

    ASSERT(value > 0);

    uint8_t chunks[10] = {0};  // 7-bit chunks of the input bits, at most 10 in uint64
    size_t outputSize = 0;
    {
        blockchainIndex_t bits = value;
        while (bits > 0) {
            // take next 7 bits from the right
            chunks[outputSize++] = bits & 0x7F;
            bits >>= 7;
        }
    }
    ASSERT(outputSize > 0);
    for (size_t i = outputSize - 1; i > 0; --i) {
        // highest bit set to 1 since more bytes follow
        uint8_t nextByte = chunks[i] | 0x80;
        if (!buffer_write_bytes(buf, &nextByte, 1)) {
            return false;
        }
    }
    // write the remaining byte, highest bit 0
    return buffer_write_bytes(buf, &chunks[0], 1);
}

static size_t deriveAddress_pointer(const addressParams_t* addressParams,
                                    uint8_t* outBuffer,
                                    size_t outSize) {
    const address_type_t addressType = addressParams->type;
    ASSERT(addressType == POINTER_KEY || addressType == POINTER_SCRIPT);
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    const uint8_t addressHeader =
        constructShelleyAddressHeader(addressType, addressParams->networkId);

    write_buffer_t out = buffer_init(outBuffer, outSize);
    {
        ASSERT(buffer_write_bytes(&out, &addressHeader, 1));
    }

    if (addressType == POINTER_KEY) {
        ASSERT(buffer_appendAddressPublicKeyHash(&out, &addressParams->paymentKeyPath));
    } else {
        ASSERT(buffer_write_bytes(&out, addressParams->paymentScriptHash, SCRIPT_HASH_LENGTH));
    }

    STATIC_ASSERT(SCRIPT_HASH_LENGTH == ADDRESS_KEY_HASH_LENGTH, "incompatible hash lengths");
    const int ADDRESS_LENGTH = 1 + ADDRESS_KEY_HASH_LENGTH;
    ASSERT(buffer_written_size(&out) == ADDRESS_LENGTH);

    {
        const blockchainPointer_t* stakingKeyBlockchainPointer =
            &addressParams->stakingKeyBlockchainPointer;
        ASSERT(buffer_appendVariableLengthUInt(&out, stakingKeyBlockchainPointer->blockIndex));
        ASSERT(buffer_appendVariableLengthUInt(&out, stakingKeyBlockchainPointer->txIndex));
        ASSERT(buffer_appendVariableLengthUInt(&out, stakingKeyBlockchainPointer->certificateIndex));
    }

    return buffer_written_size(&out);
}

static size_t deriveAddress_enterprise(const addressParams_t* addressParams,
                                       uint8_t* outBuffer,
                                       size_t outSize) {
    const address_type_t addressType = addressParams->type;
    ASSERT(addressType == ENTERPRISE_KEY || addressType == ENTERPRISE_SCRIPT);
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    const uint8_t addressHeader =
        constructShelleyAddressHeader(addressType, addressParams->networkId);

    write_buffer_t out = buffer_init(outBuffer, outSize);
    {
        ASSERT(buffer_write_bytes(&out, &addressHeader, 1));
    }

    if (addressType == ENTERPRISE_KEY) {
        ASSERT(buffer_appendAddressPublicKeyHash(&out, &addressParams->paymentKeyPath));
    } else {
        ASSERT(buffer_write_bytes(&out, addressParams->paymentScriptHash, SCRIPT_HASH_LENGTH));
    }

    // no staking data

    STATIC_ASSERT(SCRIPT_HASH_LENGTH == ADDRESS_KEY_HASH_LENGTH, "incompatible hash lengths");
    const int ADDRESS_LENGTH = 1 + ADDRESS_KEY_HASH_LENGTH;
    ASSERT(buffer_written_size(&out) == ADDRESS_LENGTH);

    return buffer_written_size(&out);
}

static size_t deriveAddress_reward(const addressParams_t* addressParams,
                                   uint8_t* outBuffer,
                                   size_t outSize) {
    const address_type_t addressType = addressParams->type;
    ASSERT(addressType == REWARD_KEY || addressType == REWARD_SCRIPT);
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    const uint8_t addressHeader =
        constructShelleyAddressHeader(addressType, addressParams->networkId);

    write_buffer_t out = buffer_init(outBuffer, outSize);
    {
        ASSERT(buffer_write_bytes(&out, &addressHeader, 1));
    }

    // no payment data

    if (addressType == REWARD_KEY) {
        const bip44_path_t* stakingKeyPath = &addressParams->stakingKeyPath;
        // stake key path expected (corresponds to reward account)
        BIP44_PRINTF(stakingKeyPath);
        TRACE("");
        ASSERT(bip44_isOrdinaryStakingKeyPath(stakingKeyPath));
        ASSERT(buffer_appendAddressPublicKeyHash(&out, stakingKeyPath));
    } else {
        ASSERT(buffer_write_bytes(&out, addressParams->stakingScriptHash, SCRIPT_HASH_LENGTH));
    }

    STATIC_ASSERT(SCRIPT_HASH_LENGTH == ADDRESS_KEY_HASH_LENGTH, "incompatible hash lengths");
    const int ADDRESS_LENGTH = 1 + ADDRESS_KEY_HASH_LENGTH;
    ASSERT(buffer_written_size(&out) == ADDRESS_LENGTH);

    return buffer_written_size(&out);
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

    write_buffer_t out = buffer_init(outBuffer, outSize);
    {
        const uint8_t addressHeader = constructShelleyAddressHeader(
            (source == REWARD_HASH_SOURCE_KEY) ? REWARD_KEY : REWARD_SCRIPT,
            networkId);
        ASSERT(buffer_write_bytes(&out, &addressHeader, 1));
        ASSERT(buffer_write_bytes(&out, hashBuffer, hashSize));
    }

    const int ADDRESS_LENGTH = REWARD_ACCOUNT_SIZE;
    ASSERT(buffer_written_size(&out) == ADDRESS_LENGTH);

    return buffer_written_size(&out);
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
bool buffer_parseAddressParams(buffer_t* buffer, addressParams_t* params) {
    // address type
    uint8_t addressType = 0;
    if (!buffer_read_u8(buffer, &addressType)) {
        return false;
    }
    params->type = addressType;
    TRACE("Address type: 0x%x", params->type);
    if (!isSupportedAddressType(params->type)) {
        return false;
    }

    // protocol magic / network id
    if (params->type == BYRON) {
        uint32_t protocolMagic = 0;
        if (!buffer_read_u32(buffer, &protocolMagic, BE)) {
            return false;
        }
        params->protocolMagic = protocolMagic;
        TRACE("Protocol magic: 0x%x", params->protocolMagic);
    } else {
        uint8_t networkId = 0;
        if (!buffer_read_u8(buffer, &networkId)) {
            return false;
        }
        params->networkId = networkId;
        TRACE("Network id: 0x%x", params->networkId);
        if (!isValidNetworkId(params->networkId)) {
            return false;
        }
    }

    // payment part
    switch (params->type) {
        case BASE_PAYMENT_KEY_STAKE_KEY:
        case BASE_PAYMENT_KEY_STAKE_SCRIPT:
        case POINTER_KEY:
        case ENTERPRISE_KEY:
        case BYRON: {
            if (!buffer_read_bip44_path(buffer, &params->paymentKeyPath)) {
                return false;
            }
            BIP44_PRINTF(&params->paymentKeyPath);
            TRACE("");
            break;
        }

        case BASE_PAYMENT_SCRIPT_STAKE_KEY:
        case BASE_PAYMENT_SCRIPT_STAKE_SCRIPT:
        case POINTER_SCRIPT:
        case ENTERPRISE_SCRIPT: {
            STATIC_ASSERT(SIZEOF(params->paymentScriptHash) == SCRIPT_HASH_LENGTH,
                          "Wrong address key hash length");
            if (!buffer_move(buffer, params->paymentScriptHash, SCRIPT_HASH_LENGTH)) {
                return false;
            }
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
    uint8_t stakingChoice = 0;
    if (!buffer_read_u8(buffer, &stakingChoice)) {
        return false;
    }
    params->stakingDataSource = stakingChoice;
    TRACE("Staking choice: 0x%x", (unsigned int) params->stakingDataSource);
    if (!isValidStakingChoice(params->stakingDataSource)) {
        return false;
    }

    // staking choice determines what to parse next
    switch (params->stakingDataSource) {
        case NO_STAKING:
            break;

        case STAKING_KEY_PATH: {
            if (!buffer_read_bip44_path(buffer, &params->stakingKeyPath)) {
                return false;
            }
            BIP44_PRINTF(&params->stakingKeyPath);
            TRACE("");
            break;
        }

        case STAKING_KEY_HASH: {
            STATIC_ASSERT(SIZEOF(params->stakingKeyHash) == ADDRESS_KEY_HASH_LENGTH,
                          "Wrong address key hash length");
            if (!buffer_move(buffer, params->stakingKeyHash, ADDRESS_KEY_HASH_LENGTH)) {
                return false;
            }
            TRACE("Stake key hash: ");
            TRACE_BUFFER(params->stakingKeyHash, SIZEOF(params->stakingKeyHash));
            break;
        }

        case STAKING_SCRIPT_HASH: {
            STATIC_ASSERT(SIZEOF(params->stakingScriptHash) == SCRIPT_HASH_LENGTH,
                          "Wrong script hash length");
            if (!buffer_move(buffer, params->stakingScriptHash, SCRIPT_HASH_LENGTH)) {
                return false;
            }
            TRACE("Stake script hash: ");
            TRACE_BUFFER(params->stakingScriptHash, SIZEOF(params->stakingScriptHash));
            break;
        }

        case BLOCKCHAIN_POINTER: {
            uint32_t blockIndex = 0, txIndex = 0, certIndex = 0;
            if (!buffer_read_u32(buffer, &blockIndex, BE)) {
                return false;
            }
            if (!buffer_read_u32(buffer, &txIndex, BE)) {
                return false;
            }
            if (!buffer_read_u32(buffer, &certIndex, BE)) {
                return false;
            }
            params->stakingKeyBlockchainPointer.blockIndex = blockIndex;
            params->stakingKeyBlockchainPointer.txIndex = txIndex;
            params->stakingKeyBlockchainPointer.certificateIndex = certIndex;
            TRACE("Stake key pointer: [%d, %d, %d]",
                  blockIndex,
                  txIndex,
                  certIndex);
            break;
        }

        default:
            ASSERT(false);
    }

    return true;
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

void rewardAccountToBuffer(const reward_account_t* rewardAccount,
                           uint8_t networkId,
                           uint8_t* rewardAccountBuffer) {
    switch (rewardAccount->keyReferenceType) {
        case KEY_REFERENCE_HASH: {
            STATIC_ASSERT(SIZEOF(rewardAccount->hashBuffer) == REWARD_ACCOUNT_SIZE,
                          "wrong reward account hash buffer size");
            memmove(rewardAccountBuffer, rewardAccount->hashBuffer, REWARD_ACCOUNT_SIZE);
            break;
        }
        case KEY_REFERENCE_PATH: {
            constructRewardAddressFromKeyPath(&rewardAccount->path,
                                              networkId,
                                              rewardAccountBuffer,
                                              REWARD_ACCOUNT_SIZE);
            break;
        }
        default:
            ASSERT(false);
    }
}
