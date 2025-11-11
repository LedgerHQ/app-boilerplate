#include "nbgl_use_case.h"
#include "ui_utils.h"
#include "mem_utils.h"
#include "mem.h"
#include "io.h"
#include "sw.h"
#include "utils/utils.h"

nbgl_contentTagValue_t *g_pairs = NULL;
nbgl_contentTagValueList_t *g_pairsList = NULL;

/**
 * Allocation tracker for UI buffers to prevent memory leaks
 * Tracks all dynamically allocated buffers for centralized cleanup
 */
#define MAX_UI_ALLOCATIONS 400

typedef struct {
    void *ptrs[MAX_UI_ALLOCATIONS];  /// Array of allocated pointers
    uint16_t count;                   /// Number of tracked allocations
} allocation_tracker_t;

static allocation_tracker_t g_allocation_tracker = {0};

/**
 * Track an allocated buffer for later cleanup
 *
 * @param ptr pointer to allocated buffer (can be NULL)
 */
void ui_track_allocation(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    if (g_allocation_tracker.count >= MAX_UI_ALLOCATIONS) {
        TRACE("WARNING: Allocation tracker full, buffer %p may not be cleaned up", ptr);
        return;
    }
    g_allocation_tracker.ptrs[g_allocation_tracker.count++] = ptr;
}

/**
 * Cleanup all tracked allocations and reset tracker
 */
void ui_cleanup_tracked_allocations(void) {
    for (uint16_t i = 0; i < g_allocation_tracker.count; i++) {
        mem_buffer_cleanup(&g_allocation_tracker.ptrs[i]);
    }
    g_allocation_tracker.count = 0;
}

/**
 * Allocate memory and automatically track it for cleanup
 * This ensures UI buffers are cleaned up even if allocated in loops
 * or on error paths where manual cleanup might be forgotten.
 *
 * @param size number of bytes to allocate
 * @return pointer to allocated memory, or NULL on failure
 */
void *ui_mem_alloc(size_t size) {
    void *ptr = app_mem_alloc(size);
    if (ptr != NULL) {
        ui_track_allocation(ptr);
    }
    return ptr;
}

/**
 * Internal Cleanup to free allocated memory and send an error status
 */
static void _cleanup(void) {
    ui_pairs_cleanup();
    io_send_sw(SW_INSUFFICIENT_MEMORY);
}

void ui_pairs_cleanup(void) {
    mem_buffer_cleanup((void **) &g_pairs);
    mem_buffer_cleanup((void **) &g_pairsList);
}

/**
 * Cleanup all UI-related allocated memory
 */
void ui_all_cleanup(void) {
    ui_cleanup_tracked_allocations();
    ui_pairs_cleanup();
}

/**
 * Initialize the buffers
 *
 * @return whether the initialization was successful
 */
bool ui_pairs_init(uint8_t nbPairs) {
    // Allocate the pairsList memory
    if (!mem_buffer_allocate((void **) &g_pairsList, sizeof(nbgl_contentTagValueList_t))) {
        goto error;
    }

    // Allocate the pairs memory (nbgl_contentTagValue_t for individual pairs, not List_t)
    if (!mem_buffer_allocate((void **) &g_pairs, nbPairs * sizeof(nbgl_contentTagValue_t))) {
        goto error;
    }
    g_pairsList->nbPairs = nbPairs;
    g_pairsList->pairs = g_pairs;
    return true;
error:
    _cleanup();
    return false;
}
