#pragma once

#include <stdbool.h>
#include "securityPolicy.h"
#include "addressUtils/bip44.h"

/**
 * Transaction display and signing UI functions
 * Handles NBGL transaction review flows
 */

/**
 * Display transaction information on the device and ask confirmation to sign.
 */
void ui_display_transaction(void);

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

/**
 * Clean up all NBGL review state (display buffers + warnings).
 * Call this once the review use case finishes but you still need the parsed
 * transaction context for the witness flow.
 */
void tx_review_cleanup(void);

/**
 * Display transaction witness for signing approval
 *
 * @param witnessPath BIP44 path for the witness
 * @param securityPolicy Security policy result
 * @param warnings Warning bits
 */
void ui_display_witness(const bip44_path_t* witnessPath,
                        security_policy_t securityPolicy,
                        warning_bits_t warnings);
