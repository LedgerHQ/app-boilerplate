#pragma once

#include "addressUtilsShelley.h"
#include "transaction/tx_output_types.h"
// #include "signTx.h"
// #include "signTxOutput.h"
// #include "signTxPoolRegistration.h"
#include "addressUtils/bech32.h"

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

