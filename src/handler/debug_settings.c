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

#ifdef DEBUG

#include <stdint.h>

#include "os.h"
#include "io.h"
#include "ledger_assert.h"
#include "buffer.h"

#include "debug_settings.h"
#include "globals.h"
#include "cardano_swo.h"
#include "cardano_settings.h"

int handler_debug_set_settings(const buffer_t *buf) {
    LEDGER_ASSERT(buf != NULL, "NULL buf");

    // Expect exactly 2 bytes of data
    if ((buf->size - buf->offset) != 2) {
        TRACE("DEBUG: Invalid data length: %d (expected 2)", buf->size - buf->offset);
        return io_send_sw(SWO_WRONG_DATA_LENGTH);
    }

    // Parse settings from buffer to respect offset
    uint8_t expert_mode = 0;
    uint8_t silent_export = 0;
    buffer_t read_buf = *buf;
    if (!buffer_read_u8(&read_buf, &expert_mode) ||
        !buffer_read_u8(&read_buf, &silent_export) ||
        read_buf.offset != read_buf.size) {
        TRACE("DEBUG: Invalid data length while reading settings");
        return io_send_sw(SWO_WRONG_DATA_LENGTH);
    }

    // Validate values (only 0x00 or 0x01 allowed)
    if ((expert_mode != SETTINGS_NO && expert_mode != SETTINGS_YES) ||
        (silent_export != SETTINGS_NO && silent_export != SETTINGS_YES)) {
        TRACE("DEBUG: Invalid setting values: expert=%d, silent=%d", expert_mode, silent_export);
        return io_send_sw(SWO_WRONG_DATA_LENGTH);
    }

    TRACE("DEBUG: Setting expert_mode=%d, silent_export=%d", expert_mode, silent_export);

    // Write to NVM to mirror the UI toggles
    nvm_write((void*)&N_storage.expert_mode_enabled, &expert_mode, sizeof(uint8_t));
    nvm_write((void*)&N_storage.silent_pubkey_export_enabled, &silent_export, sizeof(uint8_t));

    // Return current settings as confirmation (2 bytes)
    uint8_t response[2] = {
        N_storage.expert_mode_enabled,
        N_storage.silent_pubkey_export_enabled
    };

    return io_send_response_pointer(response, sizeof(response), SWO_SUCCESS);
}

#endif  // DEBUG
