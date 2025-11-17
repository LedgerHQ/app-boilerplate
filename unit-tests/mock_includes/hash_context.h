// Mock hash context header to satisfy hash.h macros
#pragma once

#include "blake2b_mocks.h"

// Define the context structure with header member expected by hash.h macros
typedef struct {
    cx_blake2b_context_t cx_ctx;
} hash_context_wrapper_t;
