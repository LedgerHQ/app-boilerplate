#pragma once

#include "transaction/tx_output_types.h"
#include "utils/utils.h"

__noinline_due_to_stack__ size_t deriveAssetFingerprintBech32(const uint8_t* policyId,
                                                              size_t policyIdSize,
                                                              const uint8_t* assetName,
                                                              size_t assetNameSize,
                                                              char* fingerprint,
                                                              size_t fingerprintMaxSize);

__noinline_due_to_stack__ bool str_formatTokenAmountOutput(const token_group_t* tokenGroup,
                                                           const uint8_t* assetNameBytes,
                                                           size_t assetNameSize,
                                                           uint64_t amount,
                                                           char* out,
                                                           size_t outSize);

__noinline_due_to_stack__ bool str_formatTokenAmountMint(const token_group_t* tokenGroup,
                                                         const uint8_t* assetNameBytes,
                                                         size_t assetNameSize,
                                                         int64_t amount,
                                                         char* out,
                                                         size_t outSize);
