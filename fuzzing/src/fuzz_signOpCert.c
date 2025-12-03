#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dispatcher.h"
#include "fuzz_helpers.h"

/**
 * Fuzzing harness for operational certificate signing.
 *
 * This harness feeds random APDU commands to the signOpCert handler,
 * testing the parser, state machine, and signing logic for robustness
 * against malformed input.
 *
 * Expected APDU format:
 *   CLA = 0xE0 (Cardano app)
 *   INS = 0x08 (SIGN_OPCERT)
 *   P1  = 0x00 (unused)
 *   P2  = 0x00 (unused)
 *   LC  = variable length
 *   DATA = opCert data
 */

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    fuzzing_reset_state();

    // Need at least: CLA (1) + INS (1) + P1 (1) + P2 (1) + LC (1) = 5 bytes minimum
    if (size < 5) {
        return 0;
    }

    // Parse APDU header from fuzzing input
    command_t cmd = {0};
    cmd.cla = data[0];
    cmd.ins = data[1];
    cmd.p1 = data[2];
    cmd.p2 = data[3];
    cmd.lc = data[4];

    // Advance pointer past header
    data += 5;
    size -= 5;

    // Validate we have enough data for the claimed length
    if (size < cmd.lc) {
        return 0;
    }

    // Allocate and copy command data
    uint8_t *cmd_data = NULL;
    if (cmd.lc > 0) {
        cmd_data = malloc(cmd.lc);
        if (cmd_data == NULL) {
            return 0;
        }
        memcpy(cmd_data, data, cmd.lc);
        cmd.data = cmd_data;
    } else {
        cmd.data = NULL;
    }

    // Call dispatcher - it will route to appropriate handler
    // The dispatcher validates CLA, INS, P1/P2 and calls handler_sign_opcert
    apdu_dispatcher(&cmd);

    // Clean up
    if (cmd_data != NULL) {
        free(cmd_data);
    }

    return 0;
}
