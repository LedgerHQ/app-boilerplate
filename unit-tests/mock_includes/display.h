#pragma once

#include <stdbool.h>
#include "securityPolicy/securityPolicy.h"

void ui_display_transaction(void);
int ui_prepare_transaction_review(void);
void tx_review_cleanup(void);
int ui_display_opcert(security_policy_t securityPolicy, warning_bits_t warnings);
int ui_display_pubkey(security_policy_t securityPolicy, warning_bits_t warnings);
void ui_display_witness(const bip44_path_t* witnessPath,
                        security_policy_t securityPolicy,
                        warning_bits_t warnings);
