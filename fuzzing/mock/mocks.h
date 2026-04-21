#pragma once

#include <stddef.h>
#include <stdint.h>

#include "fuzz_defs.h"
#include "exceptions.h"

/* Defined by the SDK semantic_system mock; declared here because the
 * framework harness (fuzz_harness.h) expects it at link time. */
extern try_context_t fuzz_exit_jump_ctx;

#define FUZZ_CTRL_SIZE 16
extern uint8_t fuzz_ctrl[FUZZ_CTRL_SIZE];

extern const uint8_t *fuzz_tail_ptr;
extern size_t fuzz_tail_len;
