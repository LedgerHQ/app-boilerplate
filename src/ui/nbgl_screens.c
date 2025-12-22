#include "nbgl_screens.h"
#include "addressUtils/bech32.h"
#include "utils/cardano_os_utils.h"
#include "utils/ipUtils.h"
#include "textUtils.h"
// #include "signTx.h"
// #include "signTxPoolRegistration.h"
#include "app_tokens/app_tokens.h"

__noinline_due_to_stack__ static void _ui_getAccountWithDescriptionScreen(
    char* accountDescription,
    const size_t accountDescriptionSize,
    const bip44_path_t* path) {
    explicit_bzero(accountDescription, accountDescriptionSize);

    ASSERT(bip44_hasOrdinaryWalletKeyPrefix(path));
    ASSERT(bip44_containsAccount(path));
    { bip44_printToStr(path, accountDescription, accountDescriptionSize); }

    {
        size_t len = strlen(accountDescription);
        ASSERT(len > 0);
        ASSERT(len + 1 < accountDescriptionSize);
    }
}

// the given path typically corresponds to an account
// if it contains anything more, we display just the whole path
void ui_getPublicKeyPathScreen(char* line1,
                               const size_t line1Size,
                               char* line2,
                               const size_t line2Size,
                               const bip44_path_t* path) {
    switch (bip44_classifyPath(path)) {
        case PATH_POOL_COLD_KEY: {
            strncpy(line1, "Cold public key", line1Size);

            explicit_bzero(line2, line2Size);
            bip44_printToStr(path, line2, line2Size);
            ASSERT(strlen(line2) + 1 < line2Size);
            return;
        }

        case PATH_ORDINARY_ACCOUNT: {
            strncpy(line1, "Public key", line1Size);
            _ui_getAccountWithDescriptionScreen(line2, line2Size, path);
            return;
        }

        default:
            strncpy(line1, "Public key", line1Size);
            explicit_bzero(line2, line2Size);
            bip44_printToStr(path, line2, line2Size);
            ASSERT(strlen(line2) + 1 < line2Size);
            return;
    }
}

void ui_getStakingKeyScreen(char* line, const size_t lineSize, const bip44_path_t* stakingPath) {
    ASSERT(bip44_isOrdinaryStakingKeyPath(stakingPath));

    explicit_bzero(line, lineSize);

    _ui_getAccountWithDescriptionScreen(line, lineSize, stakingPath);
}

void ui_getAccountScreen(char* line1,
                         const size_t line1Size,
                         char* line2,
                         const size_t line2Size,
                         const bip44_path_t* path) {
    explicit_bzero(line1, line1Size);
    explicit_bzero(line2, line2Size);

    uint32_t account = bip44_unharden(bip44_getAccount(path));
    STATIC_ASSERT(sizeof(account + 1) <= sizeof(unsigned), "oversized type for %u");
    STATIC_ASSERT(!IS_SIGNED(account + 1), "signed type for %u");
    if (bip44_hasByronPrefix(path)) {
        snprintf(line1, line1Size, "Byron account");
        snprintf(line2, line2Size, "#%u", account + 1);
    } else if (bip44_hasShelleyPrefix(path)) {
        snprintf(line1, line1Size, "Account");
        snprintf(line2, line2Size, "#%u", account + 1);
    } else {
        ASSERT(false);
    }
}

// bech32 for Shelley, base58 for Byron
void ui_getAddressScreen(char* line,
                         const size_t lineSize,
                         const uint8_t* addressBuffer,
                         size_t addressSize) {
    ASSERT(addressSize > 0);
    ASSERT(addressSize < BUFFER_SIZE_PARANOIA);

    explicit_bzero(line, lineSize);

    size_t length = humanReadableAddress(addressBuffer, addressSize, line, lineSize);
    ASSERT(length > 0);
    ASSERT(strlen(line) == length);
}

// display bech32-encoded reward account preceded by stake key derivation path (if given)
static void _getRewardAccountWithDescriptionScreen(char* line,
                                                   const size_t lineSize,
                                                   const key_reference_type_t keyReferenceType,
                                                   const bip44_path_t* path,
                                                   const uint8_t* rewardAccountBuffer) {
    explicit_bzero(line, lineSize);
    size_t descLen = 0;  // line length

    if (keyReferenceType == KEY_REFERENCE_PATH) {
        descLen += bip44_printToStr(path, line, lineSize);
    }
    {
        // add bech32-encoded reward account
        ASSERT(descLen < BIP44_PATH_STRING_SIZE_MAX);
        ASSERT(descLen + 1 < lineSize);

        if (descLen > 0) {
            // add a space after path if the path is present
            ASSERT(descLen + 2 < lineSize);
            line[descLen++] = ' ';
            line[descLen] = '\0';
        }

        {
            descLen += humanReadableAddress(rewardAccountBuffer,
                                            REWARD_ACCOUNT_SIZE,
                                            line + descLen,
                                            lineSize - descLen);
        }
        ASSERT(descLen == strlen(line));
        ASSERT(descLen + 1 < lineSize);
    }
}

// displays bech32-encoded reward account preceded by path (if given)
void ui_getRewardAccountScreen(char* firstLine,
                               const size_t firstLineSize,
                               char* secondLine,
                               const size_t secondLineSize,
                               const reward_account_t* rewardAccount,
                               uint8_t networkId) {
    // WARNING: reward account must be displayed in full (not just a key derivation path)
    // because the network id security policy relies on it

    ASSERT(isValidNetworkId(networkId));

    uint8_t rewardAccountBuffer[REWARD_ACCOUNT_SIZE] = {0};
    explicit_bzero(firstLine, firstLineSize);

    switch (rewardAccount->keyReferenceType) {
        case KEY_REFERENCE_PATH: {
            ASSERT(bip44_isOrdinaryStakingKeyPath(&rewardAccount->path));

            {
                uint32_t account = bip44_unharden(bip44_getAccount(&rewardAccount->path));
                STATIC_ASSERT(sizeof(account + 1) <= sizeof(unsigned), "oversized type for %u");
                STATIC_ASSERT(!IS_SIGNED(account + 1), "signed type for %u");
                snprintf(firstLine, firstLineSize, "Reward account #%u  ", account + 1);
            }

            constructRewardAddressFromKeyPath(&rewardAccount->path,
                                              networkId,
                                              rewardAccountBuffer,
                                              SIZEOF(rewardAccountBuffer));
            break;
        }

        case KEY_REFERENCE_HASH: {
            snprintf(firstLine, firstLineSize, "Reward account");

            STATIC_ASSERT(SIZEOF(rewardAccountBuffer) == REWARD_ACCOUNT_SIZE,
                          "wrong reward account buffer size");
            STATIC_ASSERT(SIZEOF(rewardAccount->hashBuffer) == REWARD_ACCOUNT_SIZE,
                          "wrong reward account hash buffer size");
            memmove(rewardAccountBuffer, rewardAccount->hashBuffer, REWARD_ACCOUNT_SIZE);
            break;
        }

        default:
            ASSERT(false);
    }

    {
        const size_t len = strlen(firstLine);
        ASSERT(len > 0);
        // make sure all the information is displayed to the user
        ASSERT(len + 1 < firstLineSize);
    }

    _getRewardAccountWithDescriptionScreen(secondLine,
                                           secondLineSize,
                                           rewardAccount->keyReferenceType,
                                           &rewardAccount->path,
                                           rewardAccountBuffer);
}

void ui_getPaymentInfoScreen(char* line1,
                             const size_t line1Size,
                             char* line2,
                             const size_t line2Size,
                             const addressParams_t* addressParams) {
    switch (determinePaymentChoice(addressParams->type)) {
        case PAYMENT_PATH: {
            snprintf(line1, line1Size, "Payment key path");
            explicit_bzero(line2, line2Size);
            bip44_printToStr(&addressParams->paymentKeyPath, line2, line2Size);
            ASSERT(strlen(line2) + 1 < line2Size);
            return;
        }

        case PAYMENT_SCRIPT_HASH: {
            snprintf(line1, line1Size, "Payment script hash");
            {
                const size_t len = bech32_encode("script",
                                                 addressParams->paymentScriptHash,
                                                 SIZEOF(addressParams->paymentScriptHash),
                                                 line2,
                                                 line2Size);
                ASSERT(len > 0);
                ASSERT(len + 1 < line2Size);
            }
            return;
        }

        default: {
            // includes PAYMENT_NONE
            ASSERT(false);
        }
    }
}

static const char STAKING_HEADING_PATH[] = "Staking path";
static const char STAKING_HEADING_KEY_HASH[] = "Stake key hash";
static const char STAKING_HEADING_SCRIPT_HASH[] = "Stake script hash";
static const char STAKING_HEADING_POINTER[] = "Stake key pointer";
static const char STAKING_HEADING_WARNING[] = "WARNING:";

void ui_getStakingInfoScreen(char* line1,
                             const size_t line1Size,
                             char* line2,
                             const size_t line2Size,
                             const addressParams_t* addressParams) {
    explicit_bzero(line2, line2Size);

    switch (addressParams->stakingDataSource) {
        case NO_STAKING: {
            switch (addressParams->type) {
                case BYRON:
                    strncpy(line1, STAKING_HEADING_WARNING, line1Size);
                    strncpy(line2, "Legacy Byron address\n(no staking rewards)", line2Size);
                    break;

                case ENTERPRISE_KEY:
                case ENTERPRISE_SCRIPT:
                    strncpy(line1, STAKING_HEADING_WARNING, line1Size);
                    strncpy(line2, "No staking rewards", line2Size);
                    break;

                default:
                    ASSERT(false);
            }
            break;
        }

        case STAKING_KEY_PATH: {
            strncpy(line1, STAKING_HEADING_PATH, line1Size);
            bip44_printToStr(&addressParams->stakingKeyPath, line2, line2Size);
            break;
        }

        case STAKING_KEY_HASH: {
            strncpy(line1, STAKING_HEADING_KEY_HASH, line1Size);
            {
                const size_t len = bech32_encode("stake_vkh",  // shared keys never go into address directly
                                                 addressParams->stakingKeyHash,
                                                 SIZEOF(addressParams->stakingKeyHash),
                                                 line2,
                                                 line2Size);
                ASSERT(len > 0);
                ASSERT(len + 1 < line2Size);
            }
            break;
        }

        case STAKING_SCRIPT_HASH: {
            strncpy(line1, STAKING_HEADING_SCRIPT_HASH, line1Size);
            {
                const size_t len = bech32_encode("script",
                                                 addressParams->stakingScriptHash,
                                                 SIZEOF(addressParams->stakingScriptHash),
                                                 line2,
                                                 line2Size);
                ASSERT(len > 0);
                ASSERT(len + 1 < line2Size);
            }
            break;
        }

        case BLOCKCHAIN_POINTER:
            strncpy(line1, STAKING_HEADING_POINTER, line1Size);
            printBlockchainPointerToStr(addressParams->stakingKeyBlockchainPointer,
                                        line2,
                                        line2Size);
            break;

        default:
            ASSERT(false);
    }

    ASSERT(line1 != NULL);
    ASSERT(strlen(line2) > 0);
    ASSERT(strlen(line2) + 1 < line2Size);
}

void ui_getAssetFingerprintScreen(char* line,
                                  const size_t lineSize,
                                  const token_group_t* tokenGroup,
                                  const uint8_t* assetNameBytes,
                                  size_t assetNameSize) {
    ASSERT(assetNameSize <= ASSET_NAME_SIZE_MAX);

    explicit_bzero(line, lineSize);

    deriveAssetFingerprintBech32(tokenGroup->policyId,
                                 SIZEOF(tokenGroup->policyId),
                                 assetNameBytes,
                                 assetNameSize,
                                 line,
                                 lineSize);
    ASSERT(strlen(line) + 1 < lineSize);
}


void ui_getPoolMarginScreen(char* line1,
                            size_t lineSize,
                            uint64_t marginNumerator,
                            uint64_t marginDenominator) {
    ASSERT(marginDenominator != 0);
    ASSERT(marginNumerator <= marginDenominator);
    ASSERT(marginDenominator <= MARGIN_DENOMINATOR_MAX);

    explicit_bzero(line1, lineSize);

    {
        // marginPercentage is a multiple of 1/100th of 1%, i.e. the fractional part of the
        // percentage has two digits adding marginDenominator / 2 to have a rounded result
        uint64_t marginPercentage =
            (10000 * marginNumerator + (marginDenominator / 2)) / marginDenominator;
        ASSERT(marginPercentage <= 10000);

        const unsigned int percentage = (unsigned int) marginPercentage;

        STATIC_ASSERT(sizeof(percentage) <= sizeof(unsigned), "oversized type for %u");
        STATIC_ASSERT(!IS_SIGNED(percentage), "signed type for %u");
        snprintf(line1, lineSize, "%u.%u %%", percentage / 100, percentage % 100);
        ASSERT(strlen(line1) + 1 < lineSize);
    }

    TRACE("%s", line1);
}

/* TODO
void ui_getPoolOwnerScreen(char* firstLine,
                           const size_t firstLineSize,
                           char* secondLine,
                           const size_t secondLineSize,
                           const pool_owner_t* owner,
                           uint32_t ownerIndex,
                           uint8_t networkId) {
    {
        ASSERT(isValidNetworkId(networkId));
        ASSERT(ownerIndex < POOL_MAX_OWNERS);
    }
    {
        uint8_t rewardAddress[REWARD_ACCOUNT_SIZE] = {0};

        switch (owner->keyReferenceType) {
            case KEY_REFERENCE_PATH: {
                ASSERT(bip44_isOrdinaryStakingKeyPath(&owner->path));

                constructRewardAddressFromKeyPath(&owner->path,
                                                  networkId,
                                                  rewardAddress,
                                                  SIZEOF(rewardAddress));
                break;
            }
            case KEY_REFERENCE_HASH: {
                STATIC_ASSERT(SIZEOF(owner->keyHash) == ADDRESS_KEY_HASH_LENGTH,
                              "wrong owner.keyHash size");

                constructRewardAddressFromHash(networkId,
                                               REWARD_HASH_SOURCE_KEY,
                                               owner->keyHash,
                                               SIZEOF(owner->keyHash),
                                               rewardAddress,
                                               SIZEOF(rewardAddress));
                break;
            }
            default:
                ASSERT(false);
        }

        explicit_bzero(firstLine, firstLineSize);
        STATIC_ASSERT(sizeof(ownerIndex + 1) <= sizeof(unsigned), "oversized type for %u");
        STATIC_ASSERT(!IS_SIGNED(ownerIndex + 1), "signed type for %u");
        // indexed from 0 as discuss with IOHK on Slack
        snprintf(firstLine, firstLineSize, "Owner #%u", ownerIndex);
        // make sure all the information is displayed to the user
        ASSERT(strlen(firstLine) + 1 < firstLineSize);

        _getRewardAccountWithDescriptionScreen(secondLine,
                                               secondLineSize,
                                               owner->keyReferenceType,
                                               &owner->path,
                                               rewardAddress);
    }
}
*/

// displays pool relay index
void ui_getPoolRelayScreen(char* line, const size_t lineSize, size_t relayIndex) {
    explicit_bzero(line, lineSize);
    {
#ifndef FUZZING
        // Skip size checks for fuzzing builds (size_t is 64-bit on x86_64 but 32-bit on device)
        STATIC_ASSERT(sizeof(relayIndex + 1) <= sizeof(unsigned), "oversized type for %u");
        STATIC_ASSERT(!IS_SIGNED(relayIndex + 1), "signed type for %u");
#endif
        // indexed from 0 as discussed with IOHK on Slack
        snprintf(line, lineSize, "#%u", (unsigned)relayIndex);
        // make sure all the information is displayed to the user
        ASSERT(strlen(line) + 1 < lineSize);
    }
}


/*
TODO
void ui_getInputScreen(char* line,
                       const size_t lineSize,
                       const sign_tx_transaction_input_t* input) {
    const tx_input_t* inputData = &input->input_data;
    ASSERT(SIZEOF(inputData->txHashBuffer) == TX_HASH_LENGTH);
    char txHex[2 * TX_HASH_LENGTH + 1] = {0};
    explicit_bzero(txHex, SIZEOF(txHex));

    int result = bytes_to_lowercase_hex(txHex, SIZEOF(txHex), inputData->txHashBuffer, TX_HASH_LENGTH);
    ASSERT(result == 0);  // SDK returns 0 on success, -1 if output buffer too small
    ASSERT(strlen(txHex) == 2 * TX_HASH_LENGTH);

    explicit_bzero(line, lineSize);

    snprintf(line, lineSize, "%u / %s", inputData->index, txHex);
    // make sure all the information is displayed to the user
    ASSERT(strlen(line) + 1 < lineSize);
}
*/
