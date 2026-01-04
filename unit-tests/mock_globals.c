// Mock global context for unit tests
#include "globals.h"

global_ctx_t G_context;
const internal_storage_t N_storage_real = {
    .expert_mode_enabled = 0,
    .silent_pubkey_export_enabled = 0,
    .initialized = 0,
};
bool unit_test_expert_mode_enabled = false;
