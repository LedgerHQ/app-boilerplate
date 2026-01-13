#include "ui_formatters.h"
#include "utils/utils.h"
#include "utils/textUtils.h"
#include "utils/ipUtils.h"
#include "format.h"
#include "cardano_tokens/cardano_tokens.h"
#include "cardano_constants.h"
#include "addressUtils/bech32.h"
#include <string.h>

static const char *getCertificateTypeName(certificate_type_t type) {
    switch (type) {
        case CERTIFICATE_STAKE_REGISTRATION:
            return "Stake Registration";
        case CERTIFICATE_STAKE_DEREGISTRATION:
            return "Stake Deregistration";
        case CERTIFICATE_STAKE_DELEGATION:
            return "Stake Delegation";
        case CERTIFICATE_STAKE_POOL_RETIREMENT:
            return "Pool Retirement";
        case CERTIFICATE_STAKE_REGISTRATION_CONWAY:
            return "Stake Registration (Conway)";
        case CERTIFICATE_STAKE_DEREGISTRATION_CONWAY:
            return "Stake Deregistration (Conway)";
        case CERTIFICATE_VOTE_DELEGATION:
            return "Vote Delegation";
        case CERTIFICATE_AUTHORIZE_COMMITTEE_HOT:
            return "Committee Authorization";
        case CERTIFICATE_RESIGN_COMMITTEE_COLD:
            return "Committee Resignation";
        case CERTIFICATE_DREP_REGISTRATION:
            return "DRep Registration";
        case CERTIFICATE_DREP_DEREGISTRATION:
            return "DRep Deregistration";
        case CERTIFICATE_DREP_UPDATE:
            return "DRep Update";
        case CERTIFICATE_STAKE_POOL_REGISTRATION:
            return "Pool Registration";
        default:
            LEDGER_ASSERT(false, "Unknown certificate type");
            return "Unknown";
    }
}

/**
 * Format bytes to lowercase hex string (UI version with bool return)
 */
bool format_hex_bytes(const uint8_t *bytes, size_t bytes_len, char *out, size_t outSize) {
    return bytes_to_lowercase_hex(out, outSize, bytes, bytes_len) == 0;
}

/**
 * Format uint64_t value to string (UI version with correct parameter order)
 */
bool format_uint64(uint64_t value, char *out, size_t outSize) {
    return format_u64(out, outSize, value);
}

#define WRITE_CHAR(ptr, end, c) \
    {                           \
        ASSERT(ptr + 1 <= end); \
        *ptr = (c);             \
        ptr++;                  \
    }

bool format_decimal_amount(uint64_t amount, size_t places, char *out, size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);
    ASSERT(places <= UINT8_MAX);

    explicit_bzero(out, outSize);

    char scratchBuffer[40] = {0};
    explicit_bzero(scratchBuffer, SIZEOF(scratchBuffer));
    char *ptr = scratchBuffer;
    char *end = scratchBuffer + SIZEOF(scratchBuffer);

    // We print in reverse

    // decimal digits
    for (size_t dec = 0; dec < places; dec++) {
        WRITE_CHAR(ptr, end, '0' + (amount % 10));
        amount /= 10;
    }
    if (places > 0) {
        WRITE_CHAR(ptr, end, '.');
    }
    // We want at least one iteration
    int place = 0;
    do {
        // thousands separator
        if (place && (place % 3 == 0)) {
            WRITE_CHAR(ptr, end, ',');
        }
        WRITE_CHAR(ptr, end, '0' + (amount % 10));
        amount /= 10;
        place++;
    } while (amount > 0);

    // Size without terminating character
    STATIC_ASSERT(sizeof(ptr - scratchBuffer) == sizeof(size_t), "bad size_t size");
    size_t rawSize = (size_t)(ptr - scratchBuffer);
    ASSERT(rawSize + 1 <= outSize);

    // Copy reversed & append terminator
    for (size_t i = 0; i < rawSize; i++) {
        out[i] = scratchBuffer[rawSize - 1 - i];
    }
    out[rawSize] = 0;

    // make sure all the information is displayed to the user
    ASSERT(strlen(out) == rawSize);

    return true;
}

bool format_ada_amount(uint64_t amount, char *out, size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    explicit_bzero(out, outSize);

    // TODO: Consider using format_fpu64 directly instead of format_decimal_amount
    // for consistency with other formatting code
    bool formatted = format_decimal_amount(amount, 6, out, outSize);
    ASSERT(formatted);
    const size_t rawSize = strlen(out);

    const char *suffix = " ADA";
    const size_t suffixLength = strlen(suffix);

    // make sure all the information is displayed to the user
    ASSERT(rawSize + suffixLength + 1 < outSize);

    snprintf(out + rawSize, outSize - rawSize, "%s", suffix);
    ASSERT(strlen(out) == rawSize + suffixLength);

    return true;
}

// Note: This is valid only for mainnet
static struct {
    uint64_t startSlotNumber;
    uint64_t startEpoch;
    uint64_t slotsInEpoch;
} EPOCH_SLOTS_CONFIG[] = {{4492800, 208, 432000}, {0, 0, 21600}};

static bool format_validity_boundary_mainnet(uint64_t slotNumber, char *out, size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    explicit_bzero(out, outSize);

    unsigned i = 0;
    while (slotNumber < EPOCH_SLOTS_CONFIG[i].startSlotNumber) {
        i++;
        ASSERT(i < ARRAY_LEN(EPOCH_SLOTS_CONFIG));
    }

    ASSERT(slotNumber >= EPOCH_SLOTS_CONFIG[i].startSlotNumber);

    uint64_t startSlotNumber = EPOCH_SLOTS_CONFIG[i].startSlotNumber;
    uint64_t startEpoch = EPOCH_SLOTS_CONFIG[i].startEpoch;
    uint64_t slotsInEpoch = EPOCH_SLOTS_CONFIG[i].slotsInEpoch;

    uint64_t epoch = startEpoch + (slotNumber - startSlotNumber) / slotsInEpoch;
    uint64_t slotInEpoch = (slotNumber - startSlotNumber) % slotsInEpoch;

    STATIC_ASSERT(sizeof(int) >= sizeof(uint32_t), "wrong int size");

    ASSERT(outSize > 0);  // so we can write null terminator
    if (epoch > 1000000) {
        // thousands of years
        snprintf(out, outSize, "epoch more than 1000000");
    } else {
        snprintf(out, outSize, "epoch %u / slot %u", (unsigned) epoch, (unsigned) slotInEpoch);
    }

    // snprintf does not return length written
    size_t len = strlen(out);
    // make sure we did not truncate
    ASSERT(len + 1 < outSize);

    return true;
}

bool format_validity_boundary(uint64_t slotNumber,
                              uint8_t networkId,
                              uint32_t protocolMagic,
                              char *out,
                              size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    explicit_bzero(out, outSize);

    // Determine if we can use the nicer mainnet formatting
    // Note: Epoch/slot calculations are valid only for mainnet,
    // as they depend on network params that could differ for testnets
    if ((networkId == MAINNET_NETWORK_ID) && (protocolMagic == MAINNET_PROTOCOL_MAGIC)) {
        // Use pretty formatting for mainnet (epoch / slot)
        return format_validity_boundary_mainnet(slotNumber, out, outSize);
    }

    // Use simple uint64 formatting for non-mainnet
    bool success = format_u64(out, outSize, slotNumber);
    ASSERT(success);
    size_t len = strlen(out);
    ASSERT(len + 1 < outSize);
    return true;
}

/**
 * Format pool profit margin as percentage
 */
bool format_pool_margin(uint64_t numerator, uint64_t denominator, char *out, size_t outSize) {
    // Convert to percentage (0-10000 basis points)
    uint64_t margin_percentage = (10000 * numerator + (denominator / 2)) / denominator;
    unsigned int percentage = (unsigned int) margin_percentage;

    snprintf(out, outSize, "%u.%u %%", percentage / 100, percentage % 100);
    size_t len = strlen(out);
    return len < outSize;
}

/**
 * Format 16-bit unsigned integer to string
 */
bool format_uint16(uint16_t value, char *out, size_t outSize) {
    snprintf(out, outSize, "%u", value);
    size_t len = strlen(out);
    return len < outSize;
}

/**
 * Format unsigned integer with "#" prefix for numbered items
 */
bool format_index_with_prefix(uint32_t value, char *out, size_t outSize) {
    snprintf(out, outSize, "#%u", value);
    size_t len = strlen(out);
    return len < outSize;
}

/**
 * Format IPv4 address from byte array
 */
bool format_ipv4(const ipv4_t *ipv4, char *out, size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);
    ASSERT(out != NULL);
    ASSERT(ipv4 != NULL);

    explicit_bzero(out, outSize);

    if (ipv4->isNull) {
        snprintf(out, outSize, "(none)");
    } else {
        inet_ntop4(ipv4->ip, out, outSize);
    }

    // make sure all the information is displayed to the user
    ASSERT(strlen(out) + 1 < outSize);

    return true;
}

/**
 * Format IPv6 address from byte array
 */
bool format_ipv6(const ipv6_t *ipv6, char *out, size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);
    ASSERT(out != NULL);
    ASSERT(ipv6 != NULL);

    explicit_bzero(out, outSize);

    if (ipv6->isNull) {
        snprintf(out, outSize, "(none)");
    } else {
        inet_ntop6(ipv6->ip, out, outSize);
    }

    // make sure all the information is displayed to the user
    ASSERT(strlen(out) + 1 < outSize);

    return true;
}

/**
 * Format vote option enum to string
 */
bool format_vote_option(vote_t voteOption, char *out, size_t outSize) {
    const char* vote_str;
    switch (voteOption) {
        case VOTE_NO:
            vote_str = "No";
            break;
        case VOTE_YES:
            vote_str = "Yes";
            break;
        case VOTE_ABSTAIN:
            vote_str = "Abstain";
            break;
        default:
            vote_str = "Unknown";
            break;
    }

    snprintf(out, outSize, "%s", vote_str);
    size_t len = strlen(out);
    return len < outSize;
}

/**
 * Format constant DRep values (Abstain and No Confidence)
 *
 * These are special DRep types that represent predefined voting choices,
 * not derived from key paths or hashes.
 */
bool format_constant_drep(ext_drep_type_t drep_type, char *out, size_t outSize) {
    const char* drep_str;
    switch (drep_type) {
        case EXT_DREP_ABSTAIN:
            drep_str = "Abstain";
            break;
        case EXT_DREP_NO_CONFIDENCE:
            drep_str = "No Confidence";
            break;
        default:
            LEDGER_ASSERT(false, "Only abstain and no confidence are constant DRep types");
            return false;
    }

    snprintf(out, outSize, "%s", drep_str);
    size_t len = strlen(out);
    return len < outSize;
}

/**
 * Format certificate type enum to string
 */
bool format_certificate_type(certificate_type_t type, char *out, size_t outSize) {
    const char *cert_type_name = getCertificateTypeName(type);
    snprintf(out, outSize, "%s", cert_type_name);
    size_t len = strlen(out);
    return len < outSize;
}

/**
 * Format URL from raw buffer
 *
 * Copies URL bytes and null-terminates them.
 */
bool format_url(const uint8_t *url, size_t urlLength, char *out, size_t outSize) {
    if (urlLength >= outSize) {
        return false;
    }
    STATIC_ASSERT(MAX_ANCHOR_URL_LENGTH == MAX_POOL_METADATA_URL_LENGTH, "URL length limits must match");
    ASSERT(urlLength <= MAX_ANCHOR_URL_LENGTH);
    ASSERT(str_isPrintableAsciiWithoutSpaces(url, urlLength));
    memcpy(out, url, urlLength);
    out[urlLength] = '\0';
    return true;
}

bool format_dns_name(const uint8_t *dnsName, size_t dnsLength, char *out, size_t outSize) {
    if (dnsLength >= outSize) {
        return false;
    }
    ASSERT(dnsLength <= MAX_DNS_NAME_LENGTH);
    ASSERT(str_isUnambiguousAscii(dnsName, dnsLength));
    memcpy(out, dnsName, dnsLength);
    out[dnsLength] = '\0';
    return true;
}

/**
 * Format asset fingerprint in bech32 format
 */
bool format_asset_fingerprint_bech32(const uint8_t *policyId,
                                     const uint8_t *assetName,
                                     size_t assetNameLen,
                                     char *out,
                                     size_t outSize) {
    // Derive fingerprint bytes from policy ID and asset name
    uint8_t fingerprintBuffer[20];  // ASSET_FINGERPRINT_SIZE = 20
    deriveAssetFingerprintBytes(
        policyId,
        MINTING_POLICY_ID_LENGTH,
        assetName,
        assetNameLen,
        fingerprintBuffer,
        sizeof(fingerprintBuffer));

    // Encode fingerprint bytes as bech32 with "asset" prefix
    return format_bech32("asset", fingerprintBuffer, sizeof(fingerprintBuffer), out, outSize);
}
