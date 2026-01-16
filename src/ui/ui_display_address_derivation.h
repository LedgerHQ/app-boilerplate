#pragma once

#include "securityPolicy.h"
//TODO: add warning bits
/**
 * Display address derivation
 *
 * @param securityPolicy Security policy result
 * @param warnings Warning bits
 * @return 0 if success, negative integer otherwise
 */
void ui_deriveAddress_handleDisplay(security_policy_t securityPolicy, warning_bits_t warnings);

/**
 * Return address derivation
 *
 * @param securityPolicy Security policy result
 * @param warnings Warning bits
 * @return 0 if success, negative integer otherwise
 */
void ui_deriveAddress_handleReturn(security_policy_t securityPolicy, warning_bits_t warnings);