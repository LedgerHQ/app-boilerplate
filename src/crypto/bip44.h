#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "buffer.h"

#define BIP44_MAX_PATH_ELEMENTS 5u
// each element in path is uint32, so at most 10 decimal digits
// plus ' for hardened plus / as a separator, plus the initial m and '\0'
#define BIP44_PATH_STRING_SIZE_MAX (1 + 12 * BIP44_MAX_PATH_ELEMENTS + 1)

typedef struct {
    uint32_t path[BIP44_MAX_PATH_ELEMENTS];
    uint32_t length;
} bip44_path_t;

static const uint32_t PURPOSE_BYRON = 44;
static const uint32_t PURPOSE_SHELLEY = 1852;
static const uint32_t PURPOSE_MULTISIG = 1854;

static const uint32_t PURPOSE_MINT = 1855;

static const uint32_t PURPOSE_POOL_COLD_KEY = 1853;

static const uint32_t PURPOSE_CVOTE_KEY = 1694;

static const uint32_t ADA_COIN_TYPE = 1815;

static const uint32_t HARDENED_BIP32 = ((uint32_t) 1 << 31);

bool isHardened(uint32_t value);
uint32_t harden(uint32_t value);
uint32_t unharden(uint32_t value);

// TODO hide these two?
bool bip44_check_path(bip44_path_t* pathSpec, const uint8_t* dataBuffer, size_t dataSize);
size_t bip44_parseFromWire(bip44_path_t* pathSpec, const uint8_t* dataBuffer, size_t dataSize);

bool buffer_read_bip44_path(buffer_t *buffer, bip44_path_t* path);

// Indexes into pathSpec
enum {
    // wallet keys:
    // ordinary https://cips.cardano.org/cips/cip1852/
    // multisig https://cips.cardano.org/cips/cip1854/
    // cip36 vote keys:
    // https://cips.cardano.org/cips/cip36/
    BIP44_I_PURPOSE = 0,
    BIP44_I_COIN_TYPE = 1,
    BIP44_I_ACCOUNT = 2,
    BIP44_I_CHAIN = 3,
    BIP44_I_ADDRESS = 4,
    BIP44_I_REST = 5,

    // mint keys https://cips.cardano.org/cips/cip1855/
    BIP44_I_MINT_POLICY = 2,

    // pool cold keys https://cips.cardano.org/cips/cip1853/
    BIP44_I_POOL_COLD_KEY_USECASE = 2,
    BIP44_I_POOL_COLD_KEY = 3,
};

bool bip44_hasByronPrefix(const bip44_path_t* pathSpec);
bool bip44_hasShelleyPrefix(const bip44_path_t* pathSpec);
bool bip44_hasOrdinaryWalletKeyPrefix(const bip44_path_t* pathSpec);
bool bip44_hasMultisigWalletKeyPrefix(const bip44_path_t* pathSpec);
bool bip44_hasMintKeyPrefix(const bip44_path_t* pathSpec);
bool bip44_hasPoolColdKeyPrefix(const bip44_path_t* pathSpec);
bool bip44_hasCVoteKeyPrefix(const bip44_path_t* pathSpec);

bool bip44_containsAccount(const bip44_path_t* pathSpec);
uint32_t bip44_getAccount(const bip44_path_t* pathSpec);

bool bip44_containsChainType(const bip44_path_t* pathSpec);

bool bip44_containsAddress(const bip44_path_t* pathSpec);

bool bip44_isOrdinaryStakingKeyPath(const bip44_path_t* pathSpec);
bool bip44_isMultisigStakingKeyPath(const bip44_path_t* pathSpec);
bool bip44_isMultidelegationStakingKeyPath(const bip44_path_t* pathSpec);

bool bip44_isDRepKeyPath(const bip44_path_t* pathSpec);
bool bip44_isCommitteeColdKeyPath(const bip44_path_t* pathSpec);
bool bip44_isCommitteeHotKeyPath(const bip44_path_t* pathSpec);

bool bip44_isMintKeyPath(const bip44_path_t* pathSpec);

bool bip44_isPoolColdKeyPath(const bip44_path_t* pathSpec);

bool bip44_isCVoteKeyPath(const bip44_path_t* pathSpec);

size_t bip44_printToStr(const bip44_path_t*, char* out, size_t outSize);

typedef enum {
    // hd wallet account
    PATH_ORDINARY_ACCOUNT,
    PATH_MULTISIG_ACCOUNT,

    // hd wallet address (payment part in shelley)
    PATH_ORDINARY_PAYMENT_KEY,
    PATH_MULTISIG_PAYMENT_KEY,

    // hd wallet reward address, withdrawal witness, pool owner
    PATH_ORDINARY_STAKING_KEY,
    PATH_MULTISIG_STAKING_KEY,

    // DRep key
    // m / 1852' / 1815' / account' / 3 / address_index
    PATH_DREP_KEY,

    // constitutional committee hot key
    // m / 1852' / 1815' / account' / 4 / address_index
    PATH_COMMITTEE_COLD_KEY,
    // constitutional committee cold key
    // m / 1852' / 1815' / account' / 5 / address_index
    PATH_COMMITTEE_HOT_KEY,

    // native token minting/burning
    PATH_MINT_KEY,

    // pool cold key in pool registrations and retirements
    PATH_POOL_COLD_KEY,

    // cip36 voting, incl. Catalyst
    PATH_CVOTE_ACCOUNT,
    PATH_CVOTE_KEY,

    // none of the above
    PATH_INVALID,
} bip44_path_type_t;

bip44_path_type_t bip44_classifyPath(const bip44_path_t* pathSpec);

bool bip44_isPathReasonable(const bip44_path_t* pathSpec);

void bip44_pathToKeyHash(const bip44_path_t* pathSpec,
                                                   uint8_t* hash,
                                                   size_t hashSize);

bool bip44_pathsEqual(const bip44_path_t* lhs, const bip44_path_t* rhs);

#ifdef DEVEL
void bip44_PRINTF(const bip44_path_t* pathSpec);
#define BIP44_PRINTF(PATH) bip44_PRINTF(PATH)
#else
#define BIP44_PRINTF(PATH)
#endif  // DEVEL
