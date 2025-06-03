#include "cx_errors.h"
#include "ox_ec.h"
#include "os_task.h"
#include <string.h>
#include <setjmp.h>

// APPs expect a specific length
cx_err_t cx_ecdomain_parameters_length(cx_curve_t cv, size_t *length) {
    (void) cv;
    *length = (size_t) 32;
    return 0x00000000;
}

// Simulates writing to NVM
void nvm_write(void *dst_adr, void *src_adr, unsigned int src_len) {
    if (!dst_adr || !src_adr || src_len == 0) {
        return;
    }
    memcpy(dst_adr, src_adr, src_len);
}

// to simulate exiting makes a long_jump to fuzzer harness
extern jmp_buf fuzz_exit_jump_buf;
*G_exception_context = fuzz_exit_jump_buf;
void __attribute__((noreturn)) os_sched_exit(bolos_task_status_t exit_code) {
    if (fuzz_exit_jump_buf != NULL) longjmp(fuzz_exit_jump_buf, 1);
}

void __attribute__((noreturn)) os_lib_end(void) {
    if (fuzz_exit_jump_buf != NULL) longjmp(fuzz_exit_jump_buf, 1);
}
