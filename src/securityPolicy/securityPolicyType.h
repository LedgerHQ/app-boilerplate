#pragma once

#include "os.h"

typedef enum {
    POLICY_DENY = 1,
    POLICY_SHOW = 2,  // element is displayed to the user
    POLICY_HIDE = 3   // element is silently approved (no UI)
} security_policy_t;
