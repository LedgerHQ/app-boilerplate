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

#include "os.h"
#include "io.h"

#include "get_serial.h"
#include "sw.h"
#include "utils/assert.h"

/**
 * Device serial number length as returned by os_serial().
 * Standard Ledger device serial is 7 bytes.
 */
#define SERIAL_LENGTH 7

int handler_get_serial(void) {
    uint8_t serial[SERIAL_LENGTH] = {0};

    // Get device serial from the system
    size_t len = os_serial(serial, SERIAL_LENGTH);

    // Verify we got the expected length
    ASSERT(len == SERIAL_LENGTH);

    return io_send_response_pointer(serial, SERIAL_LENGTH, SWO_SUCCESS);
}
