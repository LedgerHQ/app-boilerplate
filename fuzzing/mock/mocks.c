#include <stdarg.h>

#include "mocks.h"

uint8_t fuzz_ctrl[FUZZ_CTRL_SIZE];
const uint8_t *fuzz_tail_ptr = NULL;
size_t fuzz_tail_len = 0;

/* The SDK exclude list strips -DPRINTF(...)= for fuzz builds so app sources
 * compile PRINTF as a function call; this stub keeps it a link-time no-op. */
int PRINTF(const char *format, ...) {
    (void) format;
    return 0;
}

void os_explicit_zero_BSS_segment(void) {
    /* No-op: zeroing BSS would erase the Absolution prefix state. */
}
