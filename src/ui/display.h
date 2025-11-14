#pragma once

#include <stdbool.h>  // bool

#include "securityPolicy.h"

#if defined(TARGET_NANOX) || defined(TARGET_NANOS2)
#define ICON_APP_CARDANO     C_icon_ada_nanox
#define ICON_APP_HOME        ICON_APP_CARDANO
#define ICON_APP_WARNING     C_icon_warning
#elif defined(TARGET_STAX)
#define ICON_APP_CARDANO     C_icon_ada_stax
#define ICON_APP_HOME        ICON_APP_CARDANO
#define ICON_APP_WARNING     C_Warning_64px
#elif defined(TARGET_FLEX)
#define ICON_APP_CARDANO     C_icon_ada_flex
#define ICON_APP_HOME        ICON_APP_CARDANO
#define ICON_APP_WARNING     C_Warning_64px
#elif defined(TARGET_APEX_P)
#define ICON_APP_CARDANO     C_icon_ada_apex
#define ICON_APP_HOME        ICON_APP_CARDANO
#define ICON_APP_WARNING     LARGE_WARNING_ICON
#endif

/**
 * Callback to reuse action with approve/reject in step FLOW.
 */
typedef void (*action_validate_cb)(bool);

/**
 * Display transaction information on the device and ask confirmation to sign.
 *
 * @return 0 if success, negative integer otherwise.
 *
 */
int ui_display_transaction(void);

/**
 * Cleanup NBGL display buffers and warnings
 * Includes warning structures, g_pairs array, and tracked per-output/withdrawal strings
 * Safe to call even if warnings were never allocated (handles NULL gracefully)
 */
/**
 * Clean up all NBGL review state (display buffers + warnings).
 * Call this once the review use case finishes but you still need the parsed
 * transaction context for the witness flow.
 */
void tx_review_cleanup(void);

int ui_display_opcert(security_policy_t securityPolicy);
int ui_display_pubkey(security_policy_t securityPolicy);
int ui_display_witness(const bip44_path_t* witnessPath, security_policy_t securityPolicy);
