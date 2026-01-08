#pragma once

#include <stdbool.h>
#include "nbgl_use_case.h"
#include "utils/assert.h"

extern nbgl_contentTagValue_t *g_pairs;
extern nbgl_contentTagValueList_t *g_pairsList;

bool ui_pairs_init(uint8_t nbPairs);
void ui_pairs_cleanup(void);
uint16_t ui_pairs_get_count(void);
#ifdef __GNUC__
#define UI_STATIC_LABEL(label) ((void)sizeof(char[__builtin_constant_p(label) ? 1 : -1]), (label))
#else
#define UI_STATIC_LABEL(label) (label)
#endif

/**
 * Add a label-value pair to the UI pairs list with optional shrinking
 *
 * @param label static label string (should be constant, compile-time checked by UI_STATIC_LABEL macro)
 * @param tmp_buf temporary buffer containing the value (will be freed after use)
 * @param shrink if true, allocates exact size for the value; if false, uses buffer as-is
 * @return true on success, false on failure
 */
bool ui_pairs_add_static_label_impl(const char* label, char* tmp_buf, bool shrink);

/**
 * Add a label-value pair to the UI pairs list (legacy wrapper, always shrinks)
 * Use ui_pairs_add_static_label_impl with shrink=true for equivalent behavior
 *
 * @param label static label string (should be constant, compile-time checked by UI_STATIC_LABEL macro)
 * @param tmp_buf temporary buffer containing the value (will be freed after use)
 * @return true on success, false on failure
 */
bool ui_pairs_add_static_label(const char* label, char* tmp_buf);

/**
 * Track an allocated buffer for later cleanup
 *
 * @param ptr pointer to allocated buffer (can be NULL)
 */
void ui_track_allocation(void *ptr);

/**
 * Cleanup all tracked allocations and reset tracker
 */
void ui_cleanup_tracked_allocations(void);

/**
 * Reset UI state and cleanup all UI allocations.
 */
void ui_reset_state(void);

/**
 * Allocate memory and automatically track it for cleanup
 * This ensures UI buffers are cleaned up even if allocated in loops
 * or on error paths where manual cleanup might be forgotten.
 *
 * @param size number of bytes to allocate
 * @return pointer to allocated memory, or NULL on failure
 */
void *ui_mem_alloc(size_t size);
