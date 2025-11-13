/**
 * Dynamic allocator that uses a fixed-length buffer that is hopefully big enough
 *
 * The two functions alloc & dealloc use the buffer as a simple stack.
 * Especially useful when an unpredictable amount of data will be received and have to be stored
 * during the transaction but discarded right after.
 */

#include <stdint.h>
#include "mem.h"
#include "mem_alloc.h"
#include "os_print.h"
#include "utils/utils.h"

// TODO 24 * 1024 does not compile for Nano X
#define SIZE_MEM_BUFFER (23 * 1024)

static uint8_t mem_buffer[SIZE_MEM_BUFFER] __attribute__((aligned(sizeof(intmax_t))));
static mem_ctx_t mem_ctx = NULL;

#ifdef HAVE_MEMORY_PROFILING
#define MP_LOG_PREFIX "==MP "
#endif

bool app_mem_init(void) {
    void *buf = mem_buffer;
    size_t buf_size = sizeof(mem_buffer);

    TRACE("Initializing mem_alloc: buffer=%p, size=%d", buf, buf_size);
    mem_ctx = mem_init(buf, buf_size);
    if (mem_ctx == NULL) {
        TRACE("mem_init FAILED! buffer=%p, size=%d", buf, buf_size);
    } else {
        TRACE("mem_init SUCCESS: ctx=%p", mem_ctx);
    }
#ifdef HAVE_MEMORY_PROFILING
    PRINTF(MP_LOG_PREFIX "init;0x%p;%u\n", buf, buf_size);
#endif
    return mem_ctx != NULL;
}

void *app_mem_alloc_impl(size_t size, bool persistent, const char *file, int line) {
    void *ptr;
    TRACE("app_mem_alloc: requesting %d bytes (ctx=%p, mem_buffer=%p)", size, mem_ctx, mem_buffer);

    if (mem_ctx == NULL) {
        TRACE("ERROR: mem_ctx is NULL! Memory allocator not initialized!");
        return NULL;
    }

    ptr = mem_alloc(mem_ctx, size);
    if (ptr == NULL) {
        TRACE("app_mem_alloc: FAILED to allocate %d bytes", size);
    } else {
        TRACE("app_mem_alloc: allocated %d bytes at %p", size, ptr);
    }
#ifdef HAVE_MEMORY_PROFILING
    if (persistent) {
        PRINTF(MP_LOG_PREFIX "persist;%u;0x%p;%s:%u\n", size, ptr, file, line);
    } else {
        PRINTF(MP_LOG_PREFIX "alloc;%u;0x%p;%s:%u\n", size, ptr, file, line);
    }
#else
    (void) file;
    (void) line;
    (void) persistent;
#endif
    return ptr;
}

void app_mem_free_impl(void *ptr, const char *file, int line) {
    TRACE("app_mem_free: freeing %p", ptr);
#ifdef HAVE_MEMORY_PROFILING
    PRINTF(MP_LOG_PREFIX "free;0x%p;%s:%u\n", ptr, file, line);
#else
    (void) file;
    (void) line;
#endif
    mem_free(mem_ctx, ptr);
    TRACE("app_mem_free: freed %p", ptr);
}

void app_mem_dump_stats(void) {
    if (mem_ctx == NULL) {
        TRACE("Memory allocator not initialized");
        return;
    }

    mem_stat_t stats;
    memset(&stats, 0, sizeof(stats));
    mem_stat(mem_ctx, &stats);
    TRACE("Memory stats: total=%d, free=%d, allocated=%d, chunks=%d, allocated_chunks=%d",
          stats.total_size, stats.free_size, stats.allocated_size,
          stats.nb_chunks, stats.nb_allocated);
}
