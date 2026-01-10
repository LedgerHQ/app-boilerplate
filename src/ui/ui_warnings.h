#pragma once

#include "securityPolicy.h"
#include "nbgl_use_case.h"
#include "ui_utils.h"

/**
 * Build NBGL warning structure from warning bits.
 * Handles 0, 1, or multiple warnings.
 *
 * Memory is allocated via ui_mem_alloc and tracked for cleanup.
 *
 * @param warnings Warning bits to convert to NBGL warnings
 * @return UI_STATUS_SUCCESS on success, UI_STATUS_OUT_OF_MEMORY on failure
 */
ui_status_t ui_build_warnings(warning_bits_t warnings);

/**
 * Get the prepared warning structure for use with nbgl_useCaseAdvancedReview.
 * Must be called after ui_build_warnings().
 *
 * @return Pointer to warning structure, or NULL if no warnings
 */
const nbgl_warning_t* ui_get_warnings(void);

/**
 * Clear warning structure.
 * Called during cleanup - memory is freed via ui_cleanup_tracked_allocations.
 */
void ui_clear_warnings(void);
