#include <string.h>  // explicit_bzero

#include "globals.h"
#include "app_context.h"
#include "utils/utils.h"
#include "memory/mem.h"
#include "ui/ui_utils.h"
#include "ui/ui_warnings.h"
#include "io.h"

void reset_app_context(void) {
    TRACE("reset_app_context");
    // Clean up UI allocations and review state
    ui_cleanup_tracked_allocations();
    ui_pairs_cleanup();
    ui_clear_warnings();

    // Reset the allocator to wipe all transient memory
    LEDGER_ASSERT(app_mem_reset(), "Failed to reset memory allocator");

    // Securely zero out the entire global context
    explicit_bzero(&G_context, sizeof(G_context));

    // Ensure request type is explicitly idle
    G_context.req_type = REQUEST_NONE;
}

void send_swo_and_reset(uint16_t swo) {
    TRACE("send_swo_and_reset swo=0x%04x", swo);
    reset_app_context();
    io_send_sw(swo);
}
