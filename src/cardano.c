#include "cardano.h"
#include "addressUtilsShelley.h"

// TODO not sure why here, should be moved to addressUtils?

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
