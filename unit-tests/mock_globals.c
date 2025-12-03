// Mock global context for unit tests
#include "globals.h"

global_ctx_t G_context;
const internal_storage_t N_storage_real = {
    .expert_mode_enabled = 0,
    .silent_pubkey_export_enabled = 0,
    .initialized = 0,
};

uintptr_t pic(uintptr_t linked_address) {
    // Tests run in a flat address space, so no relocation needed.
    return linked_address;
}
