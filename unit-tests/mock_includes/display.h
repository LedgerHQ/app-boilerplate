#pragma once

#include <stdbool.h>
#include "securityPolicy/securityPolicy.h"

int ui_display_transaction(void);
void tx_review_cleanup(void);
int ui_display_opcert(security_policy_t securityPolicy);
int ui_display_pubkey(security_policy_t securityPolicy);
int ui_display_witness(const bip44_path_t* witnessPath, security_policy_t securityPolicy);
