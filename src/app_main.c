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
#include "menu.h"
#include "dispatcher.h"

global_ctx_t G_context;
APP_STORAGE_DATA_TYPE sd_cache;

#if (APP_STORAGE_DATA_STRUCT_VERSION == 2)
/* Only for demonstration purposes:
 * to be able to create a persistent data structure of previous version,
 * which is bigger and with a different order of fields.
 */
static bool init_app_storage(void) {
    bool app_data_exists = false;
    bool version_supported = false;
    if (app_storage_get_size() > 0) {
        app_data_exists = true;
        APP_STORAGE_READ_F(version, &sd_cache.version);
        if (sd_cache.version  >= APP_STORAGE_DATA_STRUCT_FIRST_SUPPORTED_VERSION) {
            version_supported = true;
        }
    }

    bool need_write = false;
    if ((!app_data_exists) || (!version_supported)) {
        /* If version is not supported let's erase all potential garbage in case the structure was bigger */
        if ((app_data_exists) && (!version_supported)) {
            app_storage_reset();
        }
        /* Start from scratch, dummy_allowed fields are already zero in the global variable */
        sd_cache.version = APP_STORAGE_DATA_STRUCT_VERSION;
        strcpy(sd_cache.string, "Boiler V2");
        need_write = true;
    } else {
        APP_STORAGE_READ_ALL(&sd_cache);
    }
    if (need_write) {
        if (APP_STORAGE_WRITE_ALL((void *) &sd_cache) != sizeof(APP_STORAGE_DATA_TYPE)) {
            PRINTF("=> storage write failure\n");
            return false;
        }
    }

    return true;
}
#endif /* #if (APP_STORAGE_DATA_STRUCT_VERSION == 2) */

#if (APP_STORAGE_DATA_STRUCT_VERSION == 3)
static bool init_app_storage(void) {

    bool app_data_exists = false;
    bool version_supported = false;
    bool conversion_needed = false;
    if (app_storage_get_size() > 0) {
        app_data_exists = true;
        APP_STORAGE_READ_F(version, &sd_cache.version);
        if (sd_cache.version  >= APP_STORAGE_DATA_STRUCT_FIRST_SUPPORTED_VERSION) {
            version_supported = true;
            if (sd_cache.version < APP_STORAGE_DATA_STRUCT_VERSION) {
                conversion_needed = true;
            }
        }
    }

    bool need_write = false;
    if ((!app_data_exists) || (!version_supported)) {
        /* If version is not supported let's erase all potential garbage in case the structure was bigger */
        if ((app_data_exists) && (!version_supported)) {
            app_storage_reset();
        }
        /* Start from scratch, dummy_allowed fields are already zero in the global variable */
        sd_cache.version = APP_STORAGE_DATA_STRUCT_VERSION;
        strcpy(sd_cache.string, "Boiler V3");
        need_write = true;
    } else {
        if (conversion_needed) {
            /* Reading previous version structure */
            app_storage_data_prev_t sd_cache_prev;
            APP_STORAGE_READ_ALL(&sd_cache_prev);
            /* Let's erase previous version data as it is bigger */
            app_storage_reset();

            sd_cache.version = APP_STORAGE_DATA_STRUCT_VERSION;
            sd_cache.dummy1_allowed = sd_cache_prev.dummy1_allowed;
            sd_cache.dummy2_allowed = sd_cache_prev.dummy2_allowed;
            strcpy(sd_cache.string, "Boiler From V2");
            need_write = true;
        }
        else {
            APP_STORAGE_READ_ALL(&sd_cache);
        }
    }
    if (need_write) {
        if (APP_STORAGE_WRITE_ALL((void *) &sd_cache) != sizeof(APP_STORAGE_DATA_TYPE)) {
            PRINTF("=> storage write failure\n");
            return false;
        }
    }

    return true;
}
#endif /* #if (APP_STORAGE_DATA_STRUCT_VERSION == 3) */

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
