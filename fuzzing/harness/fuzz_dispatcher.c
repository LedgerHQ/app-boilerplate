#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "globals.h"
#include "dispatcher.h"
#include "mocks.h"

global_ctx_t G_context;
const internal_storage_t N_storage_real;

#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
#message "Use this macro for code only needed in fuzz targets"
#endif

void print_apdu(command_t cmd) {
    printf("=> CLA=%02x, INS=%02x, P1=%02x, P2=%02x, LC=%02x, DATA=",
           cmd.cla,
           cmd.ins,
           cmd.p1,
           cmd.p2,
           cmd.lc);

    for (int i = 0; i < cmd.lc; i++) {
        printf("%02X", cmd.data[i]);
    }
    printf("\n");
}

// Fuzz entry point
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (sigsetjmp(fuzz_exit_jump_ctx.jmp_buf, 1)) return 0;

    if (size < 4) return 0;

    command_t cmd;
    cmd.cla = data[0];
    cmd.ins = data[1];
    cmd.p1 = data[2];
    cmd.p2 = data[3];
    cmd.lc = size - 4;
    if (size > 4) cmd.data = (uint8_t *) &data[4];
    // print_apdu(cmd);
    apdu_dispatcher(&cmd);
    return 0;
}
