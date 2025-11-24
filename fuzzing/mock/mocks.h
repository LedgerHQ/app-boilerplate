#include "cx_errors.h"
#include "ox_ec.h"
#include "os_task.h"
#include <string.h>
#include <setjmp.h>
#include "exceptions.h"
#include <stdio.h>
#include <stdint.h>

// to simulate exiting makes a long_jump to fuzzer harness
extern try_context_t fuzz_exit_jump_ctx;

try_context_t *try_context_get(void);

try_context_t *try_context_set(try_context_t *context);

void __attribute__((noreturn)) os_sched_exit(bolos_task_status_t exit_code);

void __attribute__((noreturn)) os_lib_end(void);

cx_err_t cx_ecdomain_parameters_length(cx_curve_t cv, size_t *length);

void nvm_write(void *dst_adr, void *src_adr, unsigned int src_len);
