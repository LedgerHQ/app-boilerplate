/*****************************************************************************
 *   Ledger App Boilerplate.
 *   (c) 2020-2025 Ledger SAS.
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
#include <string.h>  // memset, explicit_bzero

#include "os.h"
#include "ux.h"

#include "types.h"
#include "globals.h"
#include "io.h"
#include "sw.h"
#include "ui/menu.h"
#include "apdu/dispatcher.h"

global_ctx_t G_context;

static bool init_app_storage(void) {
    app_storage_data_t storage_data = {0};

    // If the Application storage content is not initialized or of a too old version, let's init it
    // from scratch
    if ((app_storage_get_size() == 0) ||
        (APP_STORAGE_READ_F(struct_version) < APP_STORAGE_DATA_STRUCT_FIRST_SUPPORTED_VERSION)) {
        // start from scratch
        storage_data.struct_version = APP_STORAGE_DATA_STRUCT_VERSION;
#if (APP_STORAGE_DATA_STRUCT_VERSION == 3)
        strcpy(storage_data.string, "Boiler V3");
#endif  // (APP_STORAGE_DATA_STRUCT_VERSION == 3)
        storage_data.dummy1_allowed = 0x00;
        storage_data.dummy2_allowed = 0x00;
        if (APP_STORAGE_WRITE_ALL((void *) &storage_data) != sizeof(storage_data)) {
            PRINTF("=> storage write failure\n");
            return false;
        }
    } else if (APP_STORAGE_READ_F(struct_version) < APP_STORAGE_DATA_STRUCT_VERSION) {
#if (APP_STORAGE_DATA_STRUCT_VERSION == 3)
        // if the version is supported and not current, let's convert it
        // In this example, only version 1 is supported as old one
        // The previous app storage data struct was:
        // typedef struct app_storage_data_s {
        //     uint8_t dummy1_allowed;
        //     uint8_t dummy2_allowed;
        // } app_storage_data_t;
        if (APP_STORAGE_READ_F(struct_version) == 2) {
            // update with new data struct version, but reuse the data version
            uint32_t version = APP_STORAGE_DATA_STRUCT_VERSION;
            APP_STORAGE_WRITE_F(struct_version, (void *) &version);
            // keep storage.string but add an initial value for storage.string
            strcpy(storage_data.string, "Boiler From V3");
            APP_STORAGE_WRITE_F(string, (void *) &storage_data.string);
        }
#endif  // (APP_STORAGE_DATA_STRUCT_VERSION == 3)
    }

    return true;
}

/**
 * Handle APDU command received and send back APDU response using handlers.
 */
void app_main() {
    // Length of APDU command received in G_io_apdu_buffer
    int input_len = 0;
    // Structured APDU command
    command_t cmd;

    io_init();

    if (!init_app_storage()) {
        PRINTF("Error while configuring the storage - aborting\n");
        return;
    }

    ui_menu_main();

    // Reset context
    explicit_bzero(&G_context, sizeof(G_context));

    for (;;) {
        // Receive command bytes in G_io_apdu_buffer
        if ((input_len = io_recv_command()) < 0) {
            PRINTF("=> io_recv_command failure\n");
            return;
        }

        // Parse APDU command from G_io_apdu_buffer
        if (!apdu_parser(&cmd, G_io_apdu_buffer, input_len)) {
            PRINTF("=> /!\\ BAD LENGTH: %.*H\n", input_len, G_io_apdu_buffer);
            io_send_sw(SW_WRONG_DATA_LENGTH);
            continue;
        }

        PRINTF("=> CLA=%02X | INS=%02X | P1=%02X | P2=%02X | Lc=%02X | CData=%.*H\n",
               cmd.cla,
               cmd.ins,
               cmd.p1,
               cmd.p2,
               cmd.lc,
               cmd.lc,
               cmd.data);

        // Dispatch structured APDU command to handler
        if (apdu_dispatcher(&cmd) < 0) {
            PRINTF("=> apdu_dispatcher failure\n");
            return;
        }
    }
}
