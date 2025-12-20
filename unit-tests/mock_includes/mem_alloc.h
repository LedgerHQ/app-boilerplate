#pragma once

#include <stddef.h>
#include <stddef.h>

typedef void* mem_ctx_t;

mem_ctx_t mem_init(void* buffer, size_t buffer_size);

void* mem_alloc(mem_ctx_t ctx, size_t size);

void mem_free(mem_ctx_t ctx, void* ptr);

void mem_dump_stats(mem_ctx_t ctx);
