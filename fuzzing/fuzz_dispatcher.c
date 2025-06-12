#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "globals.h"
#include "dispatcher.h"
#include "mock/mocks.h"
#include <setjmp.h>

global_ctx_t G_context;
const internal_storage_t N_storage_real;

jmp_buf fuzz_exit_jump_buf;

#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
#message "Use this macro for code only needed in fuzz targets"
#endif
// Fuzz entry point
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (setjmp(fuzz_exit_jump_ctx.jmp_buf) == 0 && size > 6) {
        command_t cmd;
        cmd.cla = data[0];
        cmd.ins = data[1] % 8;
        cmd.p1 = data[2];
        cmd.p2 = data[3];
        cmd.lc = data[4];

        if (size > 5 && cmd.lc > 0) {
            size_t data_len = size - 5;
            if (cmd.lc > data_len) cmd.lc = data_len;

            cmd.data = (uint8_t *) &data[5];
        } else {
            cmd.data = NULL;
        }
        apdu_dispatcher(&cmd);
    }
    return 0;
}
