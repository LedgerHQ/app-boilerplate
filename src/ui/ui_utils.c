#include "nbgl_use_case.h"
#include "ui_utils.h"
#include "mem_utils.h"
#include "io.h"
#include "sw.h"

nbgl_contentTagValue_t *g_pairs = NULL;
nbgl_contentTagValueList_t *g_pairsList = NULL;

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

    // Allocate the pairs memory
    if (!mem_buffer_allocate((void **) &g_pairs, nbPairs * sizeof(nbgl_contentTagValueList_t))) {
        goto error;
    }
    g_pairsList->nbPairs = nbPairs;
    g_pairsList->pairs = g_pairs;
    return true;
error:
    _cleanup();
    return false;
}
