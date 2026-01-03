#pragma once

#include <stdbool.h>
#include "nbgl_use_case.h"
#include "utils/assert.h"

extern nbgl_contentTagValue_t *g_pairs;
extern nbgl_contentTagValueList_t *g_pairsList;

bool ui_pairs_init(uint8_t nbPairs);
void ui_pairs_cleanup(void);
#ifdef __GNUC__
#define UI_STATIC_LABEL(label) ((void)sizeof(char[__builtin_constant_p(label) ? 1 : -1]), (label))
#else
#define UI_STATIC_LABEL(label) (label)
#endif

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
 * Allocate memory and automatically track it for cleanup
 * This ensures UI buffers are cleaned up even if allocated in loops
 * or on error paths where manual cleanup might be forgotten.
 *
 * @param size number of bytes to allocate
 * @return pointer to allocated memory, or NULL on failure
 */
void *ui_mem_alloc(size_t size);
