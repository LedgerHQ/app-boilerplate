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
    bool success = format_bip44_path(path, accountDescription, accountDescriptionSize);
    ASSERT(success);

    size_t len = strlen(accountDescription);
    ASSERT(len > 0);
    ASSERT(len + 1 < accountDescriptionSize);
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
            bool success = format_bip44_path(path, line2, line2Size);
            ASSERT(success);
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
            bool success = format_bip44_path(path, line2, line2Size);
            ASSERT(success);
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

    bool success = format_address_human_readable(addressBuffer, addressSize, line, lineSize);
    ASSERT(success);
    size_t length = strlen(line);
    ASSERT(length > 0);
    ASSERT(length + 1 < lineSize);
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
        bool pathFormatted = format_bip44_path(path, line, lineSize);
        ASSERT(pathFormatted);
        descLen += strlen(line);
    }
    {
        // add bech32-encoded reward account
        ASSERT(descLen < MAX_BIP44_PATH_STRING_LENGTH);
        ASSERT(descLen + 1 < lineSize);

        if (descLen > 0) {
            // add a space after path if the path is present
            ASSERT(descLen + 2 < lineSize);
            line[descLen++] = ' ';
            line[descLen] = '\0';
        }

        {
            bool rewardFormatted = format_address_human_readable(rewardAccountBuffer,
                                                                 REWARD_ACCOUNT_LENGTH,
                                                                 line + descLen,
                                                                 lineSize - descLen);
            ASSERT(rewardFormatted);
            descLen += strlen(line + descLen);
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

    uint8_t rewardAccountBuffer[REWARD_ACCOUNT_LENGTH] = {0};
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

            STATIC_ASSERT(SIZEOF(rewardAccountBuffer) == REWARD_ACCOUNT_LENGTH,
                          "wrong reward account buffer size");
            STATIC_ASSERT(SIZEOF(rewardAccount->hashBuffer) == REWARD_ACCOUNT_LENGTH,
                          "wrong reward account hash buffer size");
            memmove(rewardAccountBuffer, rewardAccount->hashBuffer, REWARD_ACCOUNT_LENGTH);
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
            bool success = format_bip44_path(&addressParams->paymentKeyPath, line2, line2Size);
            ASSERT(success);
            ASSERT(strlen(line2) + 1 < line2Size);
            return;
        }

        case PAYMENT_SCRIPT_HASH: {
            snprintf(line1, line1Size, "Payment script hash");
            {
                bool encoded = format_bech32("script",
                                             addressParams->paymentScriptHash,
                                             SIZEOF(addressParams->paymentScriptHash),
                                             line2,
                                             line2Size);
                ASSERT(encoded);
                ASSERT(strlen(line2) + 1 < line2Size);
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
            bool success = format_bip44_path(&addressParams->stakingKeyPath, line2, line2Size);
            ASSERT(success);
            break;
        }

        case STAKING_KEY_HASH: {
            strncpy(line1, STAKING_HEADING_KEY_HASH, line1Size);
            {
                bool encoded = format_bech32("stake_vkh",  // shared keys never go into address directly
                                             addressParams->stakingKeyHash,
                                             SIZEOF(addressParams->stakingKeyHash),
                                             line2,
                                             line2Size);
                ASSERT(encoded);
                ASSERT(strlen(line2) + 1 < line2Size);
            }
            break;
        }

        case STAKING_SCRIPT_HASH: {
            strncpy(line1, STAKING_HEADING_SCRIPT_HASH, line1Size);
            {
                bool encoded = format_bech32("script",
                                             addressParams->stakingScriptHash,
                                             SIZEOF(addressParams->stakingScriptHash),
                                             line2,
                                             line2Size);
                ASSERT(encoded);
                ASSERT(strlen(line2) + 1 < line2Size);
            }
            break;
        }

        case BLOCKCHAIN_POINTER:
            strncpy(line1, STAKING_HEADING_POINTER, line1Size);
            bool success = format_blockchain_pointer(addressParams->stakingKeyBlockchainPointer,
                                                     line2,
                                                     line2Size);
            ASSERT(success);
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
    ASSERT(assetNameSize <= MAX_ASSET_NAME_LENGTH);

    explicit_bzero(line, lineSize);

    deriveAssetFingerprintBech32(tokenGroup->policyId,
                                 SIZEOF(tokenGroup->policyId),
                                 assetNameBytes,
                                 assetNameSize,
                                 line,
                                 lineSize);
    ASSERT(strlen(line) + 1 < lineSize);
}

