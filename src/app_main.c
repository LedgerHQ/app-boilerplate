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

static void init_nvram(void) {
  Nvram_data_t storage = {0};

  // If the NVRAM content is not initialized or of a too old version, let's init it from scratch
  if (!nvram_is_initalized() || (nvram_get_struct_version() < NVRAM_FIRST_SUPPORTED_VERSION)) {
    // start at version 1 if it has never been updated
    nvram_init(1);
#if (NVRAM_STRUCT_VERSION == 2)
    strcpy(storage.string, "Boiler V2");
#endif
    storage.dummy1_allowed = 0x00;
    storage.dummy2_allowed = 0x00;
    storage.initialized = 0x01;
    nvm_write((void *) &N_nvram.data, (void *) &storage, sizeof(Nvram_data_t));
  }
  else if (nvram_get_struct_version() < NVRAM_STRUCT_VERSION) {
#if (NVRAM_STRUCT_VERSION == 2)
    // if the version is supported and not current, let's convert it
    // In this example, only version 1 is supported as old one
    // The previous nvram data struct was:
    // typedef struct Nvram_data_s {
    //     uint8_t dummy1_allowed;
    //     uint8_t dummy2_allowed;
    //     uint8_t initialized;
    // } Nvram_data_t;
    if (nvram_get_struct_version() ==  1) {
      // update header with new struct version, but reuse the data version
      nvram_init(nvram_get_data_version());
      // keep storage.string but add an initial value for storage.string
      strcpy(storage.string, "Boiler From V1");
      nvm_write((void *) &N_nvram.data.string, (void *) &storage.string, sizeof(storage.string));
    }
#endif // (NVRAM_STRUCT_VERSION == 2)
  }
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

    init_nvram();

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
