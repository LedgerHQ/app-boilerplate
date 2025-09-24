#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "ux.h"

#include "io.h"
#include "types.h"
#include "constants.h"
#include "utils/assert.h"
#include "utils/utils.h"

/**
 * Global context for user requests.
 */
extern global_ctx_t G_context;

/**
 * Global structure for NVM data storage.
 */
typedef struct internal_storage_t {
    uint8_t expert_mode_enabled;
    uint8_t silent_pubkey_export_enabled;
    uint8_t initialized;
} internal_storage_t;

extern const internal_storage_t N_storage_real;
#define N_storage (*(volatile internal_storage_t *) PIC(&N_storage_real))

// TODO needed?
#ifdef FUZZING
#define explicit_bzero(addr, size) memset((addr), 0, (size))
#endif
