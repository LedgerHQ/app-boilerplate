#include "securityPolicy/securityPolicy.h"

security_policy_t policyForDerivePrivateKey(const bip44_path_t* path) {
    switch (bip44_classifyPath(path)) {
        case PATH_ORDINARY_ACCOUNT:
        case PATH_ORDINARY_PAYMENT_KEY:
        case PATH_ORDINARY_STAKING_KEY:

        case PATH_MULTISIG_ACCOUNT:
        case PATH_MULTISIG_PAYMENT_KEY:
        case PATH_MULTISIG_STAKING_KEY:

        case PATH_DREP_KEY:
        case PATH_COMMITTEE_COLD_KEY:
        case PATH_COMMITTEE_HOT_KEY:

        case PATH_MINT_KEY:

        case PATH_POOL_COLD_KEY:

        case PATH_CVOTE_ACCOUNT:
        case PATH_CVOTE_KEY:
            return POLICY_HIDE;

        default:
            return POLICY_DENY;
    }
}
