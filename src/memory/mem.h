/*******************************************************************************
 *   Ledger Cardano App
 *   (c) 2016-2025 Ledger
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 ********************************************************************************/

#pragma once

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#ifdef HAVE_MEMORY_PROFILING
#define MP_FILE __FILE__
#define MP_LINE __LINE__
#else
#define MP_FILE NULL
#define MP_LINE 0
#endif
#define app_mem_alloc(size) app_mem_alloc_impl(size, false, MP_FILE, MP_LINE)
#define app_mem_free(ptr)   app_mem_free_impl(ptr, MP_FILE, MP_LINE)

bool app_mem_init(void);
bool app_mem_reset(void);
void *app_mem_alloc_impl(size_t size, bool persistent, const char *file, int line);
void app_mem_free_impl(void *ptr, const char *file, int line);
void app_mem_dump_stats(void);
