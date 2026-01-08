#include <stdlib.h>
#include <stdbool.h>

#include "mem_alloc.h"

mem_ctx_t mem_init(void* buffer, size_t buffer_size) {
    (void) buffer;
    (void) buffer_size;
    return (mem_ctx_t)0x1;
}

void* mem_alloc(mem_ctx_t ctx, size_t size) {
    (void) ctx;
    return malloc(size);
}

void mem_free(mem_ctx_t ctx, void* ptr) {
    (void) ctx;
    free(ptr);
}

__attribute__((weak)) bool app_mem_reset(void) {
    // Stubbed allocator reset for unit tests.
    return true;
}

void mem_dump_stats(mem_ctx_t ctx) {
    (void) ctx;
}
