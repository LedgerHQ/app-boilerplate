#pragma once

#include "os.h"

typedef enum {
    POLICY_DENY = 1,
    POLICY_SHOW = 2,  // element is displayed to the user
    POLICY_HIDE = 3   // element is silently approved (no UI)
} security_policy_t;

/*

TODO

Simplification of policies:
DENY
SHOW
HIDE

Policies are run twice:
first during parsing to check for DENY,
then later for UI (we only assert it is not DENY and just use SHOW/HIDE)

the type of confirmation button (light/ordinary) or whether to confirm
would be decided when using NBGL, not in security policy itself
there is no meaningful difference between
POLICY_PROMPT_BEFORE_RESPONSE and POLICY_PROMPT_WARN_UNUSUAL anyway

one extra argument for policies that could result in a warning
that would store the potential warnings (I guess a flist with strings?)

*/
