#pragma once

#include "addressUtilsShelley.h"
// #include "signTx.h"
// #include "signTxOutput.h"
// #include "signTxPoolRegistration.h"
#include "addressUtils/bech32.h"

__noinline_due_to_stack__ void ui_getPathScreen(char* line,
                                                const size_t lineSize,
                                                const bip44_path_t* path);

__noinline_due_to_stack__ void ui_getPublicKeyPathScreen(char* line1,
                                                         const size_t line1Size,
                                                         char* line2,
                                                         const size_t line2Size,
                                                         const bip44_path_t* path);

__noinline_due_to_stack__ void ui_getStakingKeyScreen(char* line,
                                                      const size_t lineSize,
                                                      const bip44_path_t* stakingPath);

__noinline_due_to_stack__ void ui_getAddressScreen(char* line,
                                                   const size_t lineSize,
                                                   const uint8_t* addressBuffer,
                                                   size_t addressSize);

__noinline_due_to_stack__ void ui_getAccountScreen(char* line1,
                                                   const size_t line1Size,
                                                   char* line2,
                                                   const size_t line2Size,
                                                   const bip44_path_t* path);

__noinline_due_to_stack__ void ui_getRewardAccountScreen(char* firstLine,
                                                         const size_t firstLineSize,
                                                         char* secondLine,
                                                         const size_t secondLineSize,
                                                         const reward_account_t* rewardAccount,
                                                         uint8_t networkId);

__noinline_due_to_stack__ void ui_getPaymentInfoScreen(char* line1,
                                                       const size_t line1Size,
                                                       char* line2,
                                                       const size_t line2Size,
                                                       const addressParams_t* addressParams);

__noinline_due_to_stack__ void ui_getStakingInfoScreen(char* line1,
                                                       const size_t line1Size,
                                                       char* line2,
                                                       const size_t line2Size,
                                                       const addressParams_t* addressParams);

__noinline_due_to_stack__ void ui_getAssetFingerprintScreen(char* line,
                                                            const size_t lineSize,
                                                            const token_group_t* tokenGroup,
                                                            const uint8_t* assetNameBytes,
                                                            size_t assetNameSize);

__noinline_due_to_stack__ void ui_getAdaAmountScreen(char* line,
                                                     const size_t lineSize,
                                                     uint64_t amount);

__noinline_due_to_stack__ void ui_getTokenAmountOutputScreen(char* line,
                                                             const size_t lineSize,
                                                             const token_group_t* tokenGroup,
                                                             const uint8_t* assetNameBytes,
                                                             size_t assetNameSize,
                                                             uint64_t tokenAmount);

__noinline_due_to_stack__ void ui_getTokenAmountMintScreen(char* line,
                                                           const size_t lineSize,
                                                           const token_group_t* tokenGroup,
                                                           const uint8_t* assetNameBytes,
                                                           size_t assetNameSize,
                                                           int64_t tokenAmount);
__noinline_due_to_stack__ void ui_getUint64Screen(char* line,
                                                  const size_t lineSize,
                                                  uint64_t value);

__noinline_due_to_stack__ void ui_getInt64Screen(char* line, const size_t lineSize, uint64_t value);

__noinline_due_to_stack__ void ui_getValidityBoundaryScreen(char* line,
                                                            const size_t lineSize,
                                                            uint64_t boundary,
                                                            uint8_t networkId,
                                                            uint32_t protocolMagic);

__noinline_due_to_stack__ void ui_getNetworkParamsScreen_1(char* line,
                                                           const size_t lineSize,
                                                           uint8_t networkId);

__noinline_due_to_stack__ void ui_getNetworkParamsScreen_2(char* line,
                                                           const size_t lineSize,
                                                           uint32_t protocolMagic);

__noinline_due_to_stack__ void ui_getPoolMarginScreen(char* line1,
                                                      const size_t lineSize,
                                                      uint64_t marginNumerator,
                                                      uint64_t marginDenominator);
/*
TODO
__noinline_due_to_stack__ void ui_getPoolOwnerScreen(char* firstLine,
                                                     const size_t firstLineSize,
                                                     char* secondLine,
                                                     const size_t secondLineSize,
                                                     const pool_owner_t* owner,
                                                     uint32_t ownerIndex,
                                                     uint8_t networkId);
*/
__noinline_due_to_stack__ void ui_getPoolRelayScreen(char* line,
                                                     const size_t lineSize,
                                                     size_t relayIndex);

__noinline_due_to_stack__ void ui_getIpv4Screen(char* ipStr,
                                                const size_t ipStrSize,
                                                const ipv4_t* ipv4);

__noinline_due_to_stack__ void ui_getIpv6Screen(char* ipStr,
                                                const size_t ipStrSize,
                                                const ipv6_t* ipv6);

__noinline_due_to_stack__ void ui_getIpPortScreen(char* portStr,
                                                  const size_t portStrSize,
                                                  const ipport_t* port);
/*
TODO
__noinline_due_to_stack__ void ui_getInputScreen(char* line,
                                                 const size_t lineSize,
                                                 const sign_tx_transaction_input_t* input);
*/
