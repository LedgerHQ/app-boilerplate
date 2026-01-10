#include "ui_formatters.h"
#include "tx_ui_helpers.h"
#include "utils/utils.h"
#include "utils/ipUtils.h"
#include "format.h"
#include "os.h"
#include "app_tokens/app_tokens.h"
#include "addressUtils/bech32.h"
#include "textUtils.h"
#include <string.h>

/**
 * Format bytes to lowercase hex string (UI version with bool return)
 */
bool format_hex_ui(const uint8_t *bytes, size_t bytes_len, char *out, size_t outSize) {
    return bytes_to_lowercase_hex(out, outSize, bytes, bytes_len) == 0;
}

/**
 * Format uint64_t value to string (UI version with correct parameter order)
 */
bool format_u64_ui(uint64_t value, char *out, size_t outSize) {
    return format_u64(out, outSize, value);
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
bool format_u16(uint16_t value, char *out, size_t outSize) {
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
bool format_ipv4(const uint8_t *src, char *out, size_t outSize) {
    inet_ntop4(src, out, outSize);
    // inet_ntop4 asserts internally if buffer is too small, so if we reach here it succeeded
    return true;
}

/**
 * Format IPv6 address from byte array
 */
bool format_ipv6(const uint8_t *src, char *out, size_t outSize) {
    inet_ntop6(src, out, outSize);
    // inet_ntop6 asserts internally if buffer is too small, so if we reach here it succeeded
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
 * Format anchor URL from raw buffer
 *
 * Copies anchor URL bytes and null-terminates them.
 * Matches UI_ADD_FORMAT2 signature (2 params: buffer + length).
 */
bool format_anchor_url(const uint8_t *url, size_t urlLength, char *out, size_t outSize) {
    if (urlLength >= outSize) {
        return false;
    }
    memcpy(out, url, urlLength);
    out[urlLength] = '\0';
    return true;
}

/**
 * Format asset fingerprint in bech32 format
 */
bool format_asset_fingerprint_bech32(const token_group_t *tokenGroup,
                                        const uint8_t *assetName,
                                        size_t assetNameLen,
                                        char *out,
                                        size_t outSize) {
    // Derive fingerprint bytes from policy ID and asset name
    uint8_t fingerprintBuffer[20];  // ASSET_FINGERPRINT_SIZE = 20
    deriveAssetFingerprintBytes(
        tokenGroup->policyId,
        sizeof(tokenGroup->policyId),  // MINTING_POLICY_ID_LENGTH
        assetName,
        assetNameLen,
        fingerprintBuffer,
        sizeof(fingerprintBuffer));

    // Encode fingerprint bytes as bech32 with "asset" prefix
    return format_bech32("asset", fingerprintBuffer, sizeof(fingerprintBuffer), out, outSize);
}
