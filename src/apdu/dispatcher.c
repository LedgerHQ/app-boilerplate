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

#include "dispatcher.h"
#include "../globals.h"
#include "../types.h"
#include "../io.h"
#include "../sw.h"
#include "../handler/get_version.h"
#include "../handler/get_app_name.h"

int dispatch_command(const command_t *cmd) {
    if (cmd->cla != CLA) {
        return send_sw(SW_CLA_NOT_SUPPORTED);
    }

    switch (cmd->ins) {
        case GET_VERSION:
            if (cmd->p1 != 0 || cmd->p2 != 0) {
                return send_sw(SW_WRONG_P1P2);
            }
            return get_version();
        case GET_APP_NAME:
            if (cmd->p1 != 0 || cmd->p2 != 0) {
                return send_sw(SW_WRONG_P1P2);
            }
            return get_app_name();
        default:
            return send_sw(SW_INS_NOT_SUPPORTED);
    }
}
