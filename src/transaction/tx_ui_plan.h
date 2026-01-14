/*****************************************************************************
 *   Ledger App Cardano.
 *   (c) 2025 Vacuumlabs
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *****************************************************************************/
#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * Plan for UI pair consumption and display constraints when preparing a transaction review.
 *
 * This structure is populated during transaction validation (tx_validate_and_compute_hash)
 * and consumed during UI formatting (ui_prepare_transaction_review).
 */
typedef struct {
    uint32_t pair_count;  /// Number of nbgl_contentTagValue pairs required for display

    // TODO: Detect and track transaction elements with excessive length
    // Some transaction elements are not length-limited by CDDL (e.g., metadata URLs,
    // DNS names in relays, inline datums, reference scripts). We should:
    // - Track whether any element exceeds reasonable display limits during validation
    // - Set a flag or store max element size encountered
    // - Use this during UI formatting to decide between full display vs truncation/streaming
    // - Consider if we need to enforce hard limits for security (DoS via huge fields)
    bool has_excessive_length_element;  // TODO: Implement detection during validation
} tx_ui_plan_t;
