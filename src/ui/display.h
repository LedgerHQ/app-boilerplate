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
 * Build NBGL buffers for transaction review (pairs + warnings).
 *
 * **Phase 2 of Transaction Processing:**
 * This function formats the already-validated and hashed transaction into
 * human-readable UI pairs for NBGL display.
 *
 * **Prerequisites:**
 * - Transaction must be in TX_STATE_HASHED (hash already computed by tx_validate_and_compute_hash)
 * - Security policies have already run (validation phase)
 * - UI pair count has been planned and stored in plan.pair_count
 *
 * **Implementation:** Implemented in tx_ui_format.c
 *
 * **Critical:** Must display EXACTLY the number of pairs counted in Phase 1.
 * See tx_validate.h for architecture documentation.
 *
 * @return error code if memory/resources are insufficient.
 */
int ui_prepare_transaction_review(void);

const nbgl_warning_t *ui_get_prepared_warning(void);
void ui_clear_prepared_warning(void);

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

int ui_display_opcert(security_policy_t securityPolicy, warning_bits_t warnings);
int ui_display_pubkey(security_policy_t securityPolicy, warning_bits_t warnings);
int ui_display_witness(const bip44_path_t* witnessPath,
                       security_policy_t securityPolicy,
                       warning_bits_t warnings);
