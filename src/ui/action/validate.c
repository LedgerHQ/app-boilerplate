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

#include <stdbool.h>  // bool

#include "validate.h"
#include "../../helper/send_response.h"
#include "../menu.h"
#include "../../context.h"
#include "../../sw.h"
#include "../../io.h"

void ui_action_validate_pubkey(bool choice) {
    if (choice) {
        helper_send_response_pubkey(&pk_ctx);
    } else {
        io_send_sw(SW_DENY);
    }

    ui_menu_main();
}

void ui_action_validate_amount(bool choice) {
    if (choice) {
        io_send_sw(SW_OK);
    } else {
        io_send_sw(SW_DENY);
    }

    ui_menu_main();
}
