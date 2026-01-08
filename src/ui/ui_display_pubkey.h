#pragma once

#include "securityPolicy.h"

/**
 * Display public key for export approval
 *
 * @param securityPolicy Security policy result
 * @param warnings Warning bits
 * @return 0 if success, negative integer otherwise
 */
int ui_display_pubkey(security_policy_t securityPolicy, warning_bits_t warnings);
