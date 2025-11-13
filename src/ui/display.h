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
 * Includes: g_fee, g_ttl, g_warning_msg, g_pairs array, per-output/withdrawal strings
 * Safe to call even if warnings were never allocated (handles NULL gracefully)
 */
void nbgl_display_and_warnings_cleanup(void);

/**
 * Cleanup all transaction data after UI is finished or on error paths.
 * Frees NBGL display buffers, warnings, and parsed transaction context
 * Must be called on ALL exit paths: approval, rejection, or parse errors
 */
void tx_data_cleanup(void);

int ui_display_opcert(security_policy_t securityPolicy);
int ui_display_pubkey(security_policy_t securityPolicy);
int ui_display_witness(const bip44_path_t* witnessPath, security_policy_t securityPolicy);
