#pragma once

#include <stdint.h>  // uint16_t

/**
 * Plan for UI pair consumption when preparing a transaction review.
 */
typedef struct {
    uint16_t pair_count;  /// Number of nbgl_contentTagValue pairs required
} tx_ui_plan_t;

/**
 * Compute the transaction hash while counting how many UI rows/warnings are needed.
 * Policies run once during this phase; calling code must handle POLICY_DENY responses.
 *
 * @param[out] plan Pre-allocated plan structure
 * @return status word (SWO_SUCCESS on success)
 */
int compute_tx_hash_and_plan_ui(tx_ui_plan_t* plan);
