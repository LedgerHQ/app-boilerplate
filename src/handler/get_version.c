/*****************************************************************************
 *   Ledger App Cardano.
 *   (c) 2025 Ledger SAS and Vacuumlabs
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

#include <stdint.h>  // uint*_t
#include <limits.h>  // UINT8_MAX
#include <assert.h>  // _Static_assert

#include "io.h"
#include "buffer.h"

#include "get_version.h"
#include "globals.h"
#include "cardano_swo.h"
#include "utils/assert.h"
#include "app_context.h"

void handler_get_version(const buffer_t *data_buffer) {
    ASSERT(data_buffer != NULL);

    // Verify no data is present
    if (buffer_can_read(data_buffer, 1)) {
        send_swo_and_reset(SWO_WRONG_DATA_LENGTH);
        return;
    }

    _Static_assert(APPVERSION_LEN == 3, "Length of (MAJOR || MINOR || PATCH) must be 3!");
    _Static_assert(MAJOR_VERSION >= 0 && MAJOR_VERSION <= UINT8_MAX,
                   "MAJOR version must be between 0 and 255!");
    _Static_assert(MINOR_VERSION >= 0 && MINOR_VERSION <= UINT8_MAX,
                   "MINOR version must be between 0 and 255!");
    _Static_assert(PATCH_VERSION >= 0 && PATCH_VERSION <= UINT8_MAX,
                   "PATCH version must be between 0 and 255!");

    io_send_response_pointer(
        (const uint8_t *) &(uint8_t[APPVERSION_LEN]){(uint8_t) MAJOR_VERSION,
                                                     (uint8_t) MINOR_VERSION,
                                                     (uint8_t) PATCH_VERSION},
        APPVERSION_LEN,
        SWO_SUCCESS);
}
