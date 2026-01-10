#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "transaction/tx_output_types.h"
#include "transaction/tx_certificate_types.h"
#include "transaction/tx_credential_types.h"

/**
 * Format bytes to lowercase hex string
 *
 * Wrapper around bytes_to_lowercase_hex to match the unified UI_ADD_FORMAT2 signature.
 * Returns bool (not int) to match macro expectations.
 *
 * @param bytes      Byte buffer to format as hex
 * @param bytes_len  Length of byte buffer
 * @param out        Output buffer for hex string
 * @param outSize    Size of output buffer
 * @return true on success, false on failure
 */
bool format_hex_ui(const uint8_t *bytes, size_t bytes_len, char *out, size_t outSize);

/**
 * Format a uint64_t value to string
 *
 * Wrapper around format_u64 from format.h to match UI_ADD_FORMAT1 signature.
 * SDK's format_u64 has parameters in order: (char *dst, size_t dst_len, uint64_t value)
 * UI_ADD_FORMAT1 expects: (value, char *out, size_t outSize)
 *
 * @param value      Value to format
 * @param out        Output buffer for formatted string
 * @param outSize    Size of output buffer
 * @return true on success, false on failure
 */
bool format_u64_ui(uint64_t value, char *out, size_t outSize);

/**
 * Format pool profit margin as percentage
 *
 * Converts margin numerator/denominator (in basis points) to percentage format.
 * Example: 500/10000 = 5.00%
 *
 * @param numerator    Margin numerator (basis points)
 * @param denominator  Margin denominator
 * @param out          Output buffer for formatted string
 * @param outSize      Size of output buffer
 * @return true on success, false on failure
 */
bool format_pool_margin(uint64_t numerator, uint64_t denominator, char *out, size_t outSize);

/**
 * Format a 16-bit unsigned integer
 *
 * @param value   Value to format
 * @param out     Output buffer for formatted string
 * @param outSize Size of output buffer
 * @return true on success, false on failure
 */
bool format_u16(uint16_t value, char *out, size_t outSize);

/**
 * Format an unsigned integer with "#" prefix (for numbered items)
 *
 * @param value   Value to format
 * @param out     Output buffer for formatted string
 * @param outSize Size of output buffer
 * @return true on success, false on failure
 */
bool format_index_with_prefix(uint32_t value, char *out, size_t outSize);

/**
 * Format IPv4 address from byte array
 *
 * Wrapper around inet_ntop4 to match UI_ADD_FORMAT1 signature.
 *
 * @param src     4-byte IPv4 address
 * @param out     Output buffer for formatted string
 * @param outSize Size of output buffer
 * @return true on success, false on failure
 */
bool format_ipv4(const uint8_t *src, char *out, size_t outSize);

/**
 * Format IPv6 address from byte array
 *
 * Wrapper around inet_ntop6 to match UI_ADD_FORMAT1 signature.
 *
 * @param src     16-byte IPv6 address
 * @param out     Output buffer for formatted string
 * @param outSize Size of output buffer
 * @return true on success, false on failure
 */
bool format_ipv6(const uint8_t *src, char *out, size_t outSize);

/**
 * Format vote option enum to string
 *
 * Converts vote option enum values to human-readable strings.
 * Matches UI_ADD_FORMAT1 signature for use with unified macros.
 *
 * @param voteOption Vote option enum value
 * @param out        Output buffer for formatted string
 * @param outSize    Size of output buffer
 * @return true on success, false on failure
 */
bool format_vote_option(vote_t voteOption, char *out, size_t outSize);

/**
 * Format constant DRep values (Abstain and No Confidence)
 *
 * Converts constant DRep type enum values to human-readable strings.
 * These represent predefined voting choices (not derived from key paths/hashes).
 * Matches UI_ADD_FORMAT1 signature for use with unified macros.
 *
 * @param drep_type DRep type enum value (must be EXT_DREP_ABSTAIN or EXT_DREP_NO_CONFIDENCE)
 * @param out       Output buffer for formatted string
 * @param outSize   Size of output buffer
 * @return true on success, false on failure
 */
bool format_constant_drep(ext_drep_type_t drep_type, char *out, size_t outSize);

/**
 * Format certificate type enum to string
 *
 * Converts certificate type enum values to human-readable strings.
 * Matches UI_ADD_FORMAT1 signature for use with unified macros.
 *
 * @param type    Certificate type enum value
 * @param out     Output buffer for formatted string
 * @param outSize Size of output buffer
 * @return true on success, false on failure
 */
bool format_certificate_type(certificate_type_t type, char *out, size_t outSize);

/**
 * Format anchor URL from raw buffer
 *
 * Copies anchor URL bytes and null-terminates them.
 * Matches UI_ADD_FORMAT2 signature for use with unified macros.
 *
 * @param url       Raw URL bytes (not null-terminated)
 * @param urlLength Length of URL bytes
 * @param out       Output buffer for formatted string
 * @param outSize   Size of output buffer
 * @return true on success, false on failure
 */
bool format_anchor_url(const uint8_t *url, size_t urlLength, char *out, size_t outSize);

/**
 * Format asset fingerprint in bech32 format
 *
 * Derives the fingerprint from policy ID and asset name, then encodes as bech32 with "asset" prefix.
 * Wrapper to match UI_ADD_FORMAT2 signature (tokenGroup and assetNameLen).
 *
 * @param tokenGroup       Token group containing policy ID
 * @param assetName        Asset name bytes
 * @param assetNameLen     Length of asset name
 * @param out              Output buffer for formatted bech32 string
 * @param outSize          Size of output buffer
 * @return true on success, false on failure
 */
bool format_asset_fingerprint_bech32(const token_group_t *tokenGroup,
                                        const uint8_t *assetName,
                                        size_t assetNameLen,
                                        char *out,
                                        size_t outSize);


