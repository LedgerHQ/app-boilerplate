#pragma once

#include <stdlib.h>
#include <string.h>

#include "globals.h"
#include "memory/mem.h"
#include "ui_utils.h"

static inline void fuzzing_reset_state(void) {
    // Clean up UI allocations left over from the previous iteration
    ui_cleanup_tracked_allocations();
    ui_pairs_cleanup();

    // Reset the dispatcher state to avoid cross-iteration contamination
    explicit_bzero(&G_context, sizeof(G_context));

    // Reinitialize the simple allocator so dangling pointers cannot trigger frees
    if (!app_mem_init()) {
        abort();
    }
}
