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
bool format_hex_bytes(const uint8_t *bytes, size_t bytes_len, char *out, size_t outSize);

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
bool format_uint64(uint64_t value, char *out, size_t outSize);

/**
 * Format unsigned value with fixed decimal places and thousands separators
 *
 * @param amount   Amount to format
 * @param places   Number of decimal places
 * @param out      Output buffer for formatted string
 * @param outSize  Size of output buffer
 * @return true on success, false on failure
 */
bool format_decimal_amount(uint64_t amount, size_t places, char *out, size_t outSize);

/**
 * Format ADA amount (6 decimal places + " ADA" suffix)
 *
 * @param amount   Lovelace amount
 * @param out      Output buffer for formatted string
 * @param outSize  Size of output buffer
 * @return true on success, false on failure
 */
bool format_ada_amount(uint64_t amount, char *out, size_t outSize);

/**
 * Format validity boundary (epoch/slot for mainnet, raw slot otherwise)
 *
 * @param slotNumber     Slot number to format
 * @param networkId      Network ID
 * @param protocolMagic  Protocol magic
 * @param out            Output buffer for formatted string
 * @param outSize        Size of output buffer
 * @return true on success, false on failure
 */
bool format_validity_boundary(uint64_t slotNumber,
                              uint8_t networkId,
                              uint32_t protocolMagic,
                              char *out,
                              size_t outSize);

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
bool format_uint16(uint16_t value, char *out, size_t outSize);

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
 * Format IPv4 address
 *
 * Wrapper around inet_ntop4 to match UI_ADD_FORMAT1 signature.
 *
 * @param ipv4    IPv4 address struct (can be null)
 * @param out     Output buffer for formatted string
 * @param outSize Size of output buffer
 * @return true on success, false on failure
 */
bool format_ipv4(const ipv4_t *ipv4, char *out, size_t outSize);

/**
 * Format IPv6 address
 *
 * Wrapper around inet_ntop6 to match UI_ADD_FORMAT1 signature.
 *
 * @param ipv6    IPv6 address struct (can be null)
 * @param out     Output buffer for formatted string
 * @param outSize Size of output buffer
 * @return true on success, false on failure
 */
bool format_ipv6(const ipv6_t *ipv6, char *out, size_t outSize);

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
 * Format URL from raw buffer
 *
 * Copies URL bytes and null-terminates them.
 * Matches UI_ADD_FORMAT2 signature for use with unified macros.
 *
 * @param url       Raw URL bytes (not null-terminated)
 * @param urlLength Length of URL bytes
 * @param out       Output buffer for formatted string
 * @param outSize   Size of output buffer
 * @return true on success, false on failure
 */
bool format_url(const uint8_t *url, size_t urlLength, char *out, size_t outSize);

/**
 * Format DNS name from raw buffer
 *
 * Copies DNS bytes and null-terminates them.
 * Matches UI_ADD_FORMAT2 signature for use with unified macros.
 *
 * @param dnsName   Raw DNS bytes (not null-terminated)
 * @param dnsLength Length of DNS bytes
 * @param out       Output buffer for formatted string
 * @param outSize   Size of output buffer
 * @return true on success, false on failure
 */
bool format_dns_name(const uint8_t *dnsName, size_t dnsLength, char *out, size_t outSize);

/**
 * Format asset fingerprint in bech32 format
 *
 * Derives the fingerprint from policy ID and asset name, then encodes as bech32 with "asset" prefix.
 * Wrapper to match UI_ADD_FORMAT2 signature (tokenGroup and assetNameLen).
 *
 * @param policyId         Minting policy ID
 * @param assetName        Asset name bytes
 * @param assetNameLen     Length of asset name
 * @param out              Output buffer for formatted bech32 string
 * @param outSize          Size of output buffer
 * @return true on success, false on failure
 */
bool format_asset_fingerprint_bech32(const uint8_t *policyId,
                                     const uint8_t *assetName,
                                     size_t assetNameLen,
                                     char *out,
                                     size_t outSize);
