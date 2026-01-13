#pragma once

#include "transaction/tx_output_types.h"
#include "utils/utils.h"

uint64_t abs_int64(int64_t number);

/**
 * Derive asset fingerprint bytes from policy ID and asset name
 *
 * @param policyId       Minting policy ID
 * @param policyIdSize   Size of policy ID (should be MINTING_POLICY_ID_LENGTH)
 * @param assetName      Asset name bytes
 * @param assetNameSize  Size of asset name (max MAX_ASSET_NAME_LENGTH)
 * @param fingerprintBuffer Output buffer for fingerprint bytes
 * @param fingerprintBufferSize Size of output buffer (should be ASSET_FINGERPRINT_SIZE)
 */
__noinline_due_to_stack__ void deriveAssetFingerprintBytes(const uint8_t* policyId,
                                                           size_t policyIdSize,
                                                           const uint8_t* assetName,
                                                           size_t assetNameSize,
                                                           uint8_t* fingerprintBuffer,
                                                           size_t fingerprintBufferSize);

/**
 * Format token amount for output tokens
 *
 * Formats token amount with appropriate decimal places based on token metadata.
 * Includes ticker symbol.
 *
 * @param policyId         Minting policy ID
 * @param assetNameBytes   Asset name bytes
 * @param assetNameSize    Size of asset name
 * @param amount           Token amount (unsigned)
 * @param out              Output buffer for formatted string
 * @param outSize          Size of output buffer
 * @return true on success, false on failure
 */
bool format_token_amount_output(const uint8_t *policyId,
                                const uint8_t *assetNameBytes,
                                size_t assetNameSize,
                                uint64_t amount,
                                char *out,
                                size_t outSize);

/**
 * Format token amount for minted tokens
 *
 * Formats token amount for minting operations with sign prefix (space for positive, '-' for negative).
 * Includes ticker symbol.
 *
 * @param policyId         Minting policy ID
 * @param assetNameBytes   Asset name bytes
 * @param assetNameSize    Size of asset name
 * @param amount           Token amount (signed, can be negative)
 * @param out              Output buffer for formatted string
 * @param outSize          Size of output buffer
 * @return true on success, false on failure
 */
bool format_token_amount_mint(const uint8_t *policyId,
                              const uint8_t *assetNameBytes,
                              size_t assetNameSize,
                              int64_t amount,
                              char *out,
                              size_t outSize);
