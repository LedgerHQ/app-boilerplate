/*****************************************************************************
 *   Ledger App Boilerplate.
 *   (c) 2020 Ledger SAS.
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
 *****************************************************************************/

#include <stdint.h>
#include <stdbool.h>

#include "buffer.h"
#include "io.h"
#include "ledger_assert.h"

#include "dispatcher.h"
#include "constants.h"
#include "types.h"
#include "sw.h"
#include "bench.h"
#include "read.h"

int apdu_dispatcher(const command_t *cmd) {
    LEDGER_ASSERT(cmd != NULL, "NULL cmd");

    if (cmd->cla != CLA) {
        return io_send_sw(SWO_INVALID_CLA);
    }

    switch (cmd->ins) {
        case BENCH_PRIME:
            if (cmd->lc != sizeof(uint32_t)) {
                return io_send_sw(SWO_WRONG_DATA_LENGTH);
            }
            bench_prime(read_u32_be(cmd->data, 0));
            return io_send_sw(SWO_SUCCESS);

        case BENCH_FIBO:
            if (cmd->lc != sizeof(uint32_t)) {
                return io_send_sw(SWO_WRONG_DATA_LENGTH);
            }
            bench_fibonacci(read_u32_be(cmd->data, 0));
            return io_send_sw(SWO_SUCCESS);

        default:
            return io_send_sw(SWO_INVALID_INS);
    }
}
