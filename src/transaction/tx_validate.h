#pragma once

#include <stdint.h>  // uint16_t

/**
 * Plan for UI pair consumption when preparing a transaction review.
 */
typedef struct {
    uint16_t pair_count;  /// Number of nbgl_contentTagValue pairs required
} tx_ui_plan_t;

/**
 * Validate transaction and compute its hash while counting how many UI rows/warnings are needed.
 * Policies run once during this phase; calling code must handle POLICY_DENY responses.
 *
 * @param[out] plan Pre-allocated plan structure
 * @return status word (SWO_SUCCESS on success)
 */
int tx_validate_and_compute_hash(tx_ui_plan_t* plan);
