#pragma once

#include "constants.h"
#include "types.h"
#include "addressUtils/bip44.h"
#include "utils/utils.h"

typedef enum {
    // base address contains explicit payment info (key hash / script hash)
    // and explicit staking info
    BASE_PAYMENT_KEY_STAKE_KEY = 0x0,         // 0b0000
    BASE_PAYMENT_SCRIPT_STAKE_KEY = 0x1,      // 0b0001
    BASE_PAYMENT_KEY_STAKE_SCRIPT = 0x2,      // 0b0010
    BASE_PAYMENT_SCRIPT_STAKE_SCRIPT = 0x3,   // 0b0011

    // pointer address contains explicit payment info and a pointer to blockchain for staking info
    POINTER_KEY = 0x4,                        // 0b0100
    POINTER_SCRIPT = 0x5,                     // 0b0101

    // enterprise address contains explicit payment info and no staking info
    ENTERPRISE_KEY = 0x6,                     // 0b0110
    ENTERPRISE_SCRIPT = 0x7,                  // 0b0111

    // legacy addresses, aka bootstrap addresses
    BYRON = 0x8,                              // 0b1000

    // reward address (aka reward account) contains only staking info
    REWARD_KEY = 0xE,                         // 0b1110
    REWARD_SCRIPT = 0xF,                      // 0b1111
} address_type_t;

uint8_t getAddressHeader(const uint8_t* addressBuffer, size_t addressSize);

address_type_t getAddressType(uint8_t addressHeader);
bool isSupportedAddressType(uint8_t addressHeader);
bool isShelleyAddressType(uint8_t addressType);
uint8_t constructShelleyAddressHeader(address_type_t type, uint8_t networkId);

uint8_t getNetworkId(uint8_t addressHeader);
bool isValidNetworkId(uint8_t networkId);

// describes which staking info should be incorporated into address
// (see stakingDataSource in addressParams_t)
typedef enum {
    NO_STAKING = 0x11,
    STAKING_KEY_PATH = 0x22,
    STAKING_KEY_HASH = 0x33,
    BLOCKCHAIN_POINTER = 0x44,
    STAKING_SCRIPT_HASH = 0x55,
} staking_data_source_t;

bool isValidStakingChoice(staking_data_source_t stakingDataSource);

typedef enum {
    PAYMENT_PATH,
    PAYMENT_SCRIPT_HASH,
    PAYMENT_NONE,
} payment_choice_t;

typedef uint32_t blockchainIndex_t;  // must be unsigned

typedef struct {
    blockchainIndex_t blockIndex;
    blockchainIndex_t txIndex;
    blockchainIndex_t certificateIndex;
} blockchainPointer_t;

typedef struct {
    address_type_t type;
    union {
        uint32_t protocolMagic;  // if type == BYRON
        uint8_t networkId;       // all the other types (i.e. Shelley)
    };
    union {
        bip44_path_t paymentKeyPath;
        uint8_t paymentScriptHash[SCRIPT_HASH_LENGTH];
    };
    staking_data_source_t stakingDataSource;
    union {
        bip44_path_t stakingKeyPath;
        uint8_t stakingKeyHash[ADDRESS_KEY_HASH_LENGTH];
        blockchainPointer_t stakingKeyBlockchainPointer;
        uint8_t stakingScriptHash[SCRIPT_HASH_LENGTH];
    };
} addressParams_t;

bool isStakingInfoConsistentWithAddressType(const addressParams_t* addressParams);
staking_data_source_t determineStakingChoice(address_type_t addressType);

size_t deriveAddress(const addressParams_t* addressParams, uint8_t* outBuffer, size_t outSize);

__noinline_due_to_stack__ size_t constructRewardAddressFromKeyPath(const bip44_path_t* path,
                                                                   uint8_t networkId,
                                                                   uint8_t* outBuffer,
                                                                   size_t outSize);

typedef enum {
    REWARD_HASH_SOURCE_KEY,
    REWARD_HASH_SOURCE_SCRIPT,
} reward_address_hash_source_t;

__noinline_due_to_stack__ size_t constructRewardAddressFromHash(uint8_t networkId,
                                                                reward_address_hash_source_t source,
                                                                const uint8_t* hashBuffer,
                                                                size_t hashSize,
                                                                uint8_t* outBuffer,
                                                                size_t outSize);

void printBlockchainPointerToStr(blockchainPointer_t blockchainPointer, char* out, size_t outSize);

size_t humanReadableAddress(const uint8_t* address, size_t addressSize, char* out, size_t outSize);

bool buffer_parseAddressParams(buffer_t* buffer, addressParams_t* params);

bool isValidAddressParams(const addressParams_t* addressParams);
payment_choice_t determinePaymentChoice(address_type_t addressType);

/**
 * Convert a reward account to its binary representation.
 * Handles both key path (device-owned) and key hash (third-party) references.
 *
 * @param rewardAccount The reward account structure containing either a key path or hash
 * @param networkId The network ID to use for the reward account
 * @param rewardAccountBuffer Output buffer to store the serialized reward account (REWARD_ACCOUNT_SIZE bytes)
 */
void rewardAccountToBuffer(const reward_account_t* rewardAccount,
                           uint8_t networkId,
                           uint8_t* rewardAccountBuffer);
