#pragma once

#include "os.h"

typedef enum {
    POLICY_DENY = 1,
    POLICY_ALLOW_WITHOUT_PROMPT = 2,
    POLICY_PROMPT_BEFORE_RESPONSE = 3,
    POLICY_PROMPT_WARN_UNUSUAL = 4,
    POLICY_SHOW_BEFORE_RESPONSE = 5,  // Show on display but do not ask for explicit confirmation
} security_policy_t;
