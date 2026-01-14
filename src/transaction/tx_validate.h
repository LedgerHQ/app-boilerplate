#pragma once

#include "cardano_swo.h"
#include "transaction/tx_ui_plan.h"

/**
 * @file tx_validate.h
 * @brief Transaction validation and hash computation (Phase 1 of 2-phase architecture)
 *
 * ## Two-Phase Transaction Processing Architecture
 *
 * Transaction signing is split into two distinct phases to maintain security and efficiency:
 *
 * ### Phase 1: Validation & Hash Computation (this file)
 * **File:** tx_validate.c
 * **Function:** tx_validate_and_compute_hash()
 * **Responsibilities:**
 * - Parse and validate transaction structure
 * - Run security policies on every element (may DENY unsafe transactions)
 * - Compute Blake2b-256 transaction hash (canonical CBOR serialization)
 * - Count UI pairs needed for display (planning phase)
 * - Set warning bits for unusual/dangerous patterns
 *
 * ### Phase 2: UI Formatting (tx_ui_format.h)
 * **File:** tx_ui_format.c
 * **Function:** ui_prepare_transaction_review()
 * **Responsibilities:**
 * - Format transaction elements into human-readable strings
 * - Build NBGL display pairs (key-value format)
 * - Apply formatting helpers (bech32, amounts, paths)
 * - Construct warning structures
 *
 * ## Critical Invariants
 *
 * **SYNCHRONIZATION REQUIREMENT:**
 * Both phases MUST iterate transaction elements in IDENTICAL order and count/display
 * IDENTICAL UI pairs. The actual pair count is verified at runtime (ASSERT in tx_ui_format.c).
 *
 * **When modifying code:**
 * - If you add a new displayable field in Phase 2 → update pair counting in Phase 1
 * - If you change pair count in Phase 1 → ensure Phase 2 displays exactly that many pairs
 * - Both files process elements in the same order: inputs, outputs, certificates, withdrawals, etc.
 *
 * **Example:** For device-owned outputs, Phase 1 counts +2 pairs (payment + staking info),
 * Phase 2 must call addPaymentInfoUIPairs() + addStakingInfoUIPairs().
 *
 * ## Why This Architecture?
 *
 * 1. **Single-pass efficiency:** Parse once, validate once, hash once
 * 2. **Security:** Hash is computed before UI display (prevents TOCTOU attacks)
 * 3. **Memory efficiency:** No need to store entire transaction for later hashing
 * 4. **Policy enforcement:** Security decisions made before user sees anything
 */

/**
 * Validate transaction and compute its hash while counting how many UI rows/warnings are needed.
 * Policies run once during this phase; POLICY_DENY responses call send_swo_and_reset().
 *
 * @param[out] plan Pre-allocated plan structure
 * @return SWO_SUCCESS if validation passed, otherwise an error status word
 */
int tx_validate_and_compute_hash(tx_ui_plan_t* plan);
