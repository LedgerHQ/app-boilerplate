#pragma once

#include "addressUtilsShelley.h"
#include "transaction/tx_output_types.h"
#include "transaction/tx_certificate_types.h"
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
/*
TODO
__noinline_due_to_stack__ void ui_getInputScreen(char* line,
                                                 const size_t lineSize,
                                                 const sign_tx_transaction_input_t* input);
*/
