#include "bip44.h"
#include "hash.h"
#include "keyDerivation.h"
#include "utils/utils.h"
#include "cardano_swo.h"
#include "read.h"

enum {
    CARDANO_CHAIN_EXTERNAL = 0,
    CARDANO_CHAIN_INTERNAL = 1,
    CARDANO_CHAIN_STAKING_KEY = 2,
    CARDANO_CHAIN_DREP_KEY = 3,
    CARDANO_CHAIN_COMMITTEE_COLD_KEY = 4,
    CARDANO_CHAIN_COMMITTEE_HOT_KEY = 5,
};

static const uint32_t MAX_REASONABLE_ACCOUNT = 100;
static const uint32_t MAX_REASONABLE_ADDRESS = 1000000;

static const uint32_t MAX_REASONABLE_COLD_KEY_INDEX = 1000000;
static const uint32_t MAX_REASONABLE_MINT_POLICY_INDEX = 1000000;

// Internal helper: validate BIP44 path format from wire data
// Returns true if path is valid, false on any validation error
static bool bip44_check_path(bip44_path_t* pathSpec, const uint8_t* dataBuffer, size_t dataSize) {
    if (dataSize < 1) {
        TRACE("ERROR: Invalid data size");
        return false;
    }

    size_t length = dataBuffer[0];

    if (length > ARRAY_LEN(pathSpec->path)) {
        TRACE("ERROR: Invalid path too long");
        return false;
    }
    if (length * 4 + 1 > dataSize) {
        TRACE("ERROR: Invalid path length");
        return false;
    }
    pathSpec->length = length;
    return true;
}

// Internal helper: parse BIP44 path from wire format
// Returns number of bytes consumed, or 0 on error
// Use buffer_read_bip44_path (public API) for safe reading with buffer advancement
static size_t bip44_parse_path(bip44_path_t* pathSpec, const uint8_t* dataBuffer, size_t dataSize) {
    // Check path validity - return 0 on error instead of throwing
    if (!bip44_check_path(pathSpec, dataBuffer, dataSize)) {
        return 0;
    }

    size_t offset = 1;
    for (size_t i = 0; i < pathSpec->length; i++) {
        pathSpec->path[i] = read_u32_be(dataBuffer, offset);
        offset += 4;
    }
    return offset;
}

bool buffer_read_bip44_path(buffer_t *buf, bip44_path_t* path)
{
    LEDGER_ASSERT(buf != NULL, "NULL buf");
    LEDGER_ASSERT(path != NULL, "NULL path");

    size_t length = bip44_parse_path(path, buf->ptr + buf->offset, buf->size - buf->offset);

    // bip44_parse_path returns 0 on error (invalid path)
    if (length == 0) {
        return false;
    }

    // Advance buffer by the number of bytes consumed
    if (!buffer_seek_cur(buf, length)) {
        return false;
    }

    return true;
}

bool isHardened(uint32_t value) {
    return value == (value | HARDENED_BIP32);
}

uint32_t bip44_harden(uint32_t value) {
    ASSERT(!isHardened(value));
    return value | HARDENED_BIP32;
}

uint32_t bip44_unharden(uint32_t value) {
    ASSERT(isHardened(value));
    return value & (~HARDENED_BIP32);
}

// Byron: /44'/1815'
bool bip44_hasByronPrefix(const bip44_path_t* pathSpec) {
#define CHECK(cond) \
    if (!(cond)) return false
    CHECK(pathSpec->length > BIP44_I_COIN_TYPE);
    CHECK(pathSpec->path[BIP44_I_PURPOSE] == bip44_harden(PURPOSE_BYRON));
    CHECK(pathSpec->path[BIP44_I_COIN_TYPE] == bip44_harden(ADA_COIN_TYPE));
    return true;
#undef CHECK
}

// Shelley: /1852'/1815'
bool bip44_hasShelleyPrefix(const bip44_path_t* pathSpec) {
#define CHECK(cond) \
    if (!(cond)) return false
    CHECK(pathSpec->length > BIP44_I_COIN_TYPE);
    CHECK(pathSpec->path[BIP44_I_PURPOSE] == bip44_harden(PURPOSE_SHELLEY));
    CHECK(pathSpec->path[BIP44_I_COIN_TYPE] == bip44_harden(ADA_COIN_TYPE));
    return true;
#undef CHECK
}

// /44'/1815' or /1852'/1815'
bool bip44_hasOrdinaryWalletKeyPrefix(const bip44_path_t* pathSpec) {
    return bip44_hasByronPrefix(pathSpec) || bip44_hasShelleyPrefix(pathSpec);
}

// /1854'/1815'
bool bip44_hasMultisigWalletKeyPrefix(const bip44_path_t* pathSpec) {
#define CHECK(cond) \
    if (!(cond)) return false
    CHECK(pathSpec->length > BIP44_I_COIN_TYPE);
    CHECK(pathSpec->path[BIP44_I_PURPOSE] == bip44_harden(PURPOSE_MULTISIG));
    CHECK(pathSpec->path[BIP44_I_COIN_TYPE] == bip44_harden(ADA_COIN_TYPE));
    return true;
#undef CHECK
}

// /1855'/1815'
bool bip44_hasMintKeyPrefix(const bip44_path_t* pathSpec) {
#define CHECK(cond) \
    if (!(cond)) return false
    CHECK(pathSpec->length > BIP44_I_COIN_TYPE);
    CHECK(pathSpec->path[BIP44_I_PURPOSE] == bip44_harden(PURPOSE_MINT));
    CHECK(pathSpec->path[BIP44_I_COIN_TYPE] == bip44_harden(ADA_COIN_TYPE));
    return true;
#undef CHECK
}

// /1853'/1815'
bool bip44_hasPoolColdKeyPrefix(const bip44_path_t* pathSpec) {
#define CHECK(cond) \
    if (!(cond)) return false
    CHECK(pathSpec->length > BIP44_I_COIN_TYPE);
    CHECK(pathSpec->path[BIP44_I_PURPOSE] == bip44_harden(PURPOSE_POOL_COLD_KEY));
    CHECK(pathSpec->path[BIP44_I_COIN_TYPE] == bip44_harden(ADA_COIN_TYPE));
    return true;
#undef CHECK
}

// /1694'/1815'
bool bip44_hasCVoteKeyPrefix(const bip44_path_t* pathSpec) {
#define CHECK(cond) \
    if (!(cond)) return false
    CHECK(pathSpec->length > BIP44_I_COIN_TYPE);
    CHECK(pathSpec->path[BIP44_I_PURPOSE] == bip44_harden(PURPOSE_CVOTE_KEY));
    CHECK(pathSpec->path[BIP44_I_COIN_TYPE] == bip44_harden(ADA_COIN_TYPE));
    return true;
#undef CHECK
}

// Account

bool bip44_containsAccount(const bip44_path_t* pathSpec) {
    return pathSpec->length > BIP44_I_ACCOUNT;
}

uint32_t bip44_getAccount(const bip44_path_t* pathSpec) {
    ASSERT(pathSpec->length > BIP44_I_ACCOUNT);
    return pathSpec->path[BIP44_I_ACCOUNT];
}

uint32_t bip44_getMintPolicy(const bip44_path_t* pathSpec) {
    ASSERT(pathSpec->length > BIP44_I_MINT_POLICY);
    return pathSpec->path[BIP44_I_MINT_POLICY];
}

uint32_t bip44_getColdKeyIndex(const bip44_path_t* pathSpec) {
    ASSERT(pathSpec->length > BIP44_I_POOL_COLD_KEY);
    return pathSpec->path[BIP44_I_POOL_COLD_KEY];
}

static bool bip44_hasReasonableAccount(const bip44_path_t* pathSpec) {
    if (!bip44_containsAccount(pathSpec)) return false;
    uint32_t account = bip44_getAccount(pathSpec);
    if (!isHardened(account)) return false;
    return bip44_unharden(account) <= MAX_REASONABLE_ACCOUNT;
}

static bool bip44_hasReasonableMintPolicy(const bip44_path_t* pathSpec) {
    if (!bip44_isMintKeyPath(pathSpec)) return false;
    uint32_t mintPolicyIndex = bip44_getMintPolicy(pathSpec);

    if (!isHardened(mintPolicyIndex)) return false;
    return bip44_unharden(mintPolicyIndex) <= MAX_REASONABLE_MINT_POLICY_INDEX;
}

static bool bip44_hasReasonablePoolColdKeyIndex(const bip44_path_t* pathSpec) {
    if (!bip44_isPoolColdKeyPath(pathSpec)) return false;
    uint32_t coldKeyIndex = bip44_getColdKeyIndex(pathSpec);

    if (!isHardened(coldKeyIndex)) return false;
    return bip44_unharden(coldKeyIndex) <= MAX_REASONABLE_COLD_KEY_INDEX;
}

// ChainType

bool bip44_containsChainType(const bip44_path_t* pathSpec) {
    return pathSpec->length > BIP44_I_CHAIN;
}

uint32_t bip44_getChainTypeValue(const bip44_path_t* pathSpec) {
    ASSERT(pathSpec->length > BIP44_I_CHAIN);
    return pathSpec->path[BIP44_I_CHAIN];
}

// Address

bool bip44_containsAddress(const bip44_path_t* pathSpec) {
    return pathSpec->length > BIP44_I_ADDRESS;
}

uint32_t bip44_getAddressValue(const bip44_path_t* pathSpec) {
    ASSERT(pathSpec->length > BIP44_I_ADDRESS);
    return pathSpec->path[BIP44_I_ADDRESS];
}

static bool bip44_hasReasonableAddress(const bip44_path_t* pathSpec) {
    if (!bip44_containsAddress(pathSpec)) return false;
    const uint32_t address = bip44_getAddressValue(pathSpec);
    return (address <= MAX_REASONABLE_ADDRESS);
}

static bool bip44_isConwayPathRecommended(const bip44_path_t* pathSpec) {
    switch (bip44_classifyPath(pathSpec)) {
        case PATH_DREP_KEY:
        case PATH_COMMITTEE_COLD_KEY:
        case PATH_COMMITTEE_HOT_KEY:
            // strongly recommended in CIP-0105 to only use 0 as address
            return (bip44_getAddressValue(pathSpec) == 0);
        default:
            ASSERT(false);
            return false;
    }
}

static bool bip44_containsMoreThanAddress(const bip44_path_t* pathSpec) {
    return (pathSpec->length > BIP44_I_ADDRESS + 1);
}

// stake keys
bool bip44_isOrdinaryStakingKeyPath(const bip44_path_t* pathSpec) {
#define CHECK(cond) \
    if (!(cond)) return false
    CHECK(bip44_containsAddress(pathSpec));
    CHECK(!bip44_containsMoreThanAddress(pathSpec));
    CHECK(bip44_hasShelleyPrefix(pathSpec));
    CHECK(isHardened(bip44_getAccount(pathSpec)));
    CHECK(bip44_getChainTypeValue(pathSpec) == CARDANO_CHAIN_STAKING_KEY);
    CHECK(!isHardened(bip44_getAddressValue(pathSpec)));
    return true;
#undef CHECK
}

// multisig stake keys
bool bip44_isMultisigStakingKeyPath(const bip44_path_t* pathSpec) {
#define CHECK(cond) \
    if (!(cond)) return false
    CHECK(bip44_containsAddress(pathSpec));
    CHECK(!bip44_containsMoreThanAddress(pathSpec));
    CHECK(bip44_hasMultisigWalletKeyPrefix(pathSpec));
    CHECK(isHardened(bip44_getAccount(pathSpec)));
    CHECK(bip44_getChainTypeValue(pathSpec) == CARDANO_CHAIN_STAKING_KEY);
    CHECK(!isHardened(bip44_getAddressValue(pathSpec)));
    return true;
#undef CHECK
}

bool bip44_isMultidelegationStakingKeyPath(const bip44_path_t* pathSpec) {
    return (bip44_isOrdinaryStakingKeyPath(pathSpec) || bip44_isMultisigStakingKeyPath(pathSpec)) &&
           (bip44_getAddressValue(pathSpec) > 0);
}

bool bip44_isDRepKeyPath(const bip44_path_t* pathSpec) {
#define CHECK(cond) \
    if (!(cond)) return false
    CHECK(bip44_containsAddress(pathSpec));
    CHECK(!bip44_containsMoreThanAddress(pathSpec));
    CHECK(bip44_hasShelleyPrefix(pathSpec));
    CHECK(isHardened(bip44_getAccount(pathSpec)));
    CHECK(bip44_getChainTypeValue(pathSpec) == CARDANO_CHAIN_DREP_KEY);
    // is it strongly recommended (but not forbidden) to only use 0 as address
    CHECK(!isHardened(bip44_getAddressValue(pathSpec)));
    return true;
#undef CHECK
}

bool bip44_isCommitteeColdKeyPath(const bip44_path_t* pathSpec) {
#define CHECK(cond) \
    if (!(cond)) return false
    CHECK(bip44_containsAddress(pathSpec));
    CHECK(!bip44_containsMoreThanAddress(pathSpec));
    CHECK(bip44_hasShelleyPrefix(pathSpec));
    CHECK(isHardened(bip44_getAccount(pathSpec)));
    CHECK(bip44_getChainTypeValue(pathSpec) == CARDANO_CHAIN_COMMITTEE_COLD_KEY);
    // is it strongly recommended (but not forbidden) to only use 0 as address
    CHECK(!isHardened(bip44_getAddressValue(pathSpec)));
    return true;
#undef CHECK
}

bool bip44_isCommitteeHotKeyPath(const bip44_path_t* pathSpec) {
#define CHECK(cond) \
    if (!(cond)) return false
    CHECK(bip44_containsAddress(pathSpec));
    CHECK(!bip44_containsMoreThanAddress(pathSpec));
    CHECK(bip44_hasShelleyPrefix(pathSpec));
    CHECK(isHardened(bip44_getAccount(pathSpec)));
    CHECK(bip44_getChainTypeValue(pathSpec) == CARDANO_CHAIN_COMMITTEE_HOT_KEY);
    // is it strongly recommended (but not forbidden) to only use 0 as address
    CHECK(!isHardened(bip44_getAddressValue(pathSpec)));
    return true;
#undef CHECK
}

bool bip44_isMintKeyPath(const bip44_path_t* pathSpec) {
#define CHECK(cond) \
    if (!(cond)) return false
    CHECK(pathSpec->length == BIP44_I_MINT_POLICY + 1);
    CHECK(bip44_hasMintKeyPrefix(pathSpec));
    CHECK(isHardened(pathSpec->path[BIP44_I_MINT_POLICY]));
    return true;
#undef CHECK
}

bool bip44_isPoolColdKeyPath(const bip44_path_t* pathSpec) {
#define CHECK(cond) \
    if (!(cond)) return false
    CHECK(pathSpec->length == BIP44_I_POOL_COLD_KEY + 1);
    CHECK(bip44_hasPoolColdKeyPrefix(pathSpec));
    CHECK(pathSpec->path[BIP44_I_POOL_COLD_KEY_USECASE] == bip44_harden(0));
    CHECK(isHardened(pathSpec->path[BIP44_I_POOL_COLD_KEY]));
    return true;
#undef CHECK
}

bool bip44_isCVoteKeyPath(const bip44_path_t* pathSpec) {
#define CHECK(cond) \
    if (!(cond)) return false
    CHECK(pathSpec->length == BIP44_I_ADDRESS + 1);
    CHECK(bip44_hasCVoteKeyPrefix(pathSpec));
    CHECK(bip44_getAccount(pathSpec) >= HARDENED_BIP32);
    CHECK(pathSpec->path[BIP44_I_CHAIN] == 0);  // in the future, more might be allowed
    CHECK(!isHardened(bip44_getAddressValue(pathSpec)));
    return true;
#undef CHECK
}

// returns success/failure (length must be obtained via strlen)
bool format_bip44_path(const bip44_path_t* pathSpec, char* out, size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    explicit_bzero(out, outSize);

    // we need space for the terminating \0
    // and one more byte to check whether
    // everything was printed
    ASSERT(outSize >= MAX_BIP44_PATH_STRING_LENGTH + 1);
    char* ptr = out;
    char* end = (out + outSize);

#define WRITE(fmt, ...)                                                               \
    {                                                                                 \
        ASSERT(ptr <= end);                                                           \
        STATIC_ASSERT(sizeof(end - ptr) == sizeof(size_t), "bad size_t size");        \
        size_t availableSize = (size_t)(end - ptr);                                   \
        /* Note(ppershing): We do not bother checking return */                       \
        /* value of snprintf as it always returns 0. */                               \
        /* Go figure ... */                                                           \
        snprintf(ptr, availableSize, fmt, ##__VA_ARGS__);                             \
        size_t res = strlen(ptr);                                                     \
        /* if snprintf filled all the remaining space, there is no space for '\0', */ \
        /* or the information is not displayed in full, */                            \
        /* and that's a serious security risk */                                      \
        ASSERT(res + 1 < availableSize);                                              \
        ptr += res;                                                                   \
    }

    WRITE("m");

    ASSERT(pathSpec->length <= ARRAY_LEN(pathSpec->path));

    for (size_t i = 0; i < pathSpec->length; i++) {
        const uint32_t value = pathSpec->path[i];

        if (isHardened(value)) {
            WRITE("/%u'", bip44_unharden(value));
        } else {
            WRITE("/%u", value);
        }
    }
#undef WRITE
    ASSERT(ptr >= out);
    ASSERT(ptr + 1 < end);
    const size_t resultLen = (size_t)(ptr - out);
    ASSERT(resultLen + 1 < outSize);
    ASSERT(strlen(out) == resultLen);

    return true;
}

static bip44_path_type_t bip44_classifyOrdinaryWalletPath(const bip44_path_t* pathSpec) {
    ASSERT(bip44_hasOrdinaryWalletKeyPrefix(pathSpec));

    // account must be hardened
    if (!bip44_containsAccount(pathSpec)) {
        return PATH_INVALID;
    }
    if (!isHardened(bip44_getAccount(pathSpec))) {
        return PATH_INVALID;
    }

    switch (pathSpec->length) {
        case 3: {
            return PATH_ORDINARY_ACCOUNT;
        }
        case 5: {
            const uint8_t chainType = bip44_getChainTypeValue(pathSpec);
            switch (chainType) {
                case CARDANO_CHAIN_INTERNAL:
                case CARDANO_CHAIN_EXTERNAL:
                    // We do not exclude hardened address index for legacy reasons;
                    // such indices have been allowed since Byron and no one really knows if they
                    // are in use, so we don't want to make users' funds on such addresses
                    // unavailable. But such addresses are given a warning and are never hidden from
                    // users (see bip44_isPathReasonable).
                    return PATH_ORDINARY_PAYMENT_KEY;

                case CARDANO_CHAIN_STAKING_KEY:
                    return bip44_isOrdinaryStakingKeyPath(pathSpec) ? PATH_ORDINARY_STAKING_KEY
                                                                    : PATH_INVALID;

                case CARDANO_CHAIN_DREP_KEY:
                    return bip44_isDRepKeyPath(pathSpec) ? PATH_DREP_KEY : PATH_INVALID;

                case CARDANO_CHAIN_COMMITTEE_COLD_KEY:
                    return bip44_isCommitteeColdKeyPath(pathSpec) ? PATH_COMMITTEE_COLD_KEY
                                                                  : PATH_INVALID;

                case CARDANO_CHAIN_COMMITTEE_HOT_KEY:
                    return bip44_isCommitteeHotKeyPath(pathSpec) ? PATH_COMMITTEE_HOT_KEY
                                                                 : PATH_INVALID;

                default:
                    return PATH_INVALID;
            }
        }
        default:
            return PATH_INVALID;
    }
}

static bip44_path_type_t bip44_classifyMultisigWalletPath(const bip44_path_t* pathSpec) {
    ASSERT(bip44_hasMultisigWalletKeyPrefix(pathSpec));

    // account must be hardened
    if (!bip44_containsAccount(pathSpec)) {
        return PATH_INVALID;
    }
    if (!isHardened(bip44_getAccount(pathSpec))) {
        return PATH_INVALID;
    }

    switch (pathSpec->length) {
        case 3: {
            return PATH_MULTISIG_ACCOUNT;
        }
        case 5: {
            const uint8_t chainType = bip44_getChainTypeValue(pathSpec);
            switch (chainType) {
                case CARDANO_CHAIN_EXTERNAL:
                    if (isHardened(bip44_getAddressValue(pathSpec))) {
                        // address index must not be hardened (CIP 1854)
                        return PATH_INVALID;
                    }
                    return PATH_MULTISIG_PAYMENT_KEY;

                case CARDANO_CHAIN_STAKING_KEY:
                    return bip44_isMultisigStakingKeyPath(pathSpec) ? PATH_MULTISIG_STAKING_KEY
                                                                    : PATH_INVALID;

                default:
                    return PATH_INVALID;
            }
        }
        default:
            return PATH_INVALID;
    }
}

static bip44_path_type_t bip44_classifyCVotePath(const bip44_path_t* pathSpec) {
    ASSERT(bip44_hasCVoteKeyPrefix(pathSpec));

    // account must be hardened
    if (!bip44_containsAccount(pathSpec)) {
        return PATH_INVALID;
    }
    if (!isHardened(bip44_getAccount(pathSpec))) {
        return PATH_INVALID;
    }

    switch (pathSpec->length) {
        case 3: {
            return PATH_CVOTE_ACCOUNT;
        }
        case 5: {
            return bip44_isCVoteKeyPath(pathSpec) ? PATH_CVOTE_KEY : PATH_INVALID;
        }
        default:
            return PATH_INVALID;
    }
}

bip44_path_type_t bip44_classifyPath(const bip44_path_t* pathSpec) {
    if (bip44_hasOrdinaryWalletKeyPrefix(pathSpec)) {
        return bip44_classifyOrdinaryWalletPath(pathSpec);
    }

    if (bip44_hasMultisigWalletKeyPrefix(pathSpec)) {
        return bip44_classifyMultisigWalletPath(pathSpec);
    }

    if (bip44_hasMintKeyPrefix(pathSpec)) {
        if (bip44_isMintKeyPath(pathSpec)) {
            return PATH_MINT_KEY;
        } else {
            return PATH_INVALID;
        }
    }

    if (bip44_hasPoolColdKeyPrefix(pathSpec)) {
        if (bip44_isPoolColdKeyPath(pathSpec)) {
            return PATH_POOL_COLD_KEY;
        } else {
            return PATH_INVALID;
        }
    }

    if (bip44_hasCVoteKeyPrefix(pathSpec)) {
        return bip44_classifyCVotePath(pathSpec);
    }

    return PATH_INVALID;
}

bool bip44_isPathReasonable(const bip44_path_t* pathSpec) {
    switch (bip44_classifyPath(pathSpec)) {
        case PATH_ORDINARY_ACCOUNT:
        case PATH_MULTISIG_ACCOUNT:
            return bip44_hasReasonableAccount(pathSpec);

        case PATH_ORDINARY_PAYMENT_KEY:
        case PATH_MULTISIG_PAYMENT_KEY:
            return bip44_hasReasonableAccount(pathSpec) && bip44_hasReasonableAddress(pathSpec);

        case PATH_ORDINARY_STAKING_KEY:
        case PATH_MULTISIG_STAKING_KEY:
            return bip44_hasReasonableAccount(pathSpec) && bip44_hasReasonableAddress(pathSpec);

        case PATH_DREP_KEY:
        case PATH_COMMITTEE_COLD_KEY:
        case PATH_COMMITTEE_HOT_KEY:
            return bip44_hasReasonableAccount(pathSpec) && bip44_hasReasonableAddress(pathSpec) &&
                   bip44_isConwayPathRecommended(pathSpec);

        case PATH_MINT_KEY:
            return bip44_hasReasonableMintPolicy(pathSpec);

        case PATH_POOL_COLD_KEY:
            return bip44_hasReasonablePoolColdKeyIndex(pathSpec);

        case PATH_CVOTE_ACCOUNT:
            return bip44_hasReasonableAccount(pathSpec);

        case PATH_CVOTE_KEY:
            return bip44_hasReasonableAccount(pathSpec) && bip44_hasReasonableAddress(pathSpec);

        default:
            // we are not supposed to call this for invalid paths
            ASSERT(false);
    }
    return false;
}

void bip44_pathToKeyHash(const bip44_path_t* pathSpec, uint8_t* hash, size_t hashSize) {
    ASSERT(hashSize < BUFFER_SIZE_PARANOIA);

    extendedPublicKey_t extPubKey;
    deriveExtendedPublicKey(pathSpec, &extPubKey);

    switch (hashSize) {
        case 28:
            ASSERT(hashSize * 8 == 224);

            blake2b_224_hash(extPubKey.pubKey, SIZEOF(extPubKey.pubKey), hash, hashSize);
            return;

        default:
            ASSERT(false);
    }
}

bool bip44_pathsEqual(const bip44_path_t* lhs, const bip44_path_t* rhs) {
    if (lhs->length != rhs->length) {
        return false;
    }
    for (unsigned i = 0; i < lhs->length; ++i) {
        if (lhs->path[i] != rhs->path[i]) {
            return false;
        }
    }
    return true;
}

#ifdef HAVE_PRINTF
void bip44_PRINTF(const bip44_path_t* pathSpec) {
    char tmp[MAX_BIP44_PATH_STRING_LENGTH + 1] = {0};
    bool success = format_bip44_path(pathSpec, tmp, SIZEOF(tmp));
    ASSERT(success);
    TRACE("%s", tmp);
}
#endif  // HAVE_PRINTF
