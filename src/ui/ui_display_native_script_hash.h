#pragma once

#include "securityPolicy.h"
//TODO: add warning bits
/**
 * Display native script hash
 *
 * @param securityPolicy Security policy result
 * @param warnings Warning bits
 * @return 0 if success, negative integer otherwise
 */
int ui_display_native_script_hash(security_policy_t securityPolicy);