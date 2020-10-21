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
#include "types.h"
#include "io.h"
#include "sw.h"
#include "handlers/get_version.h"
#include "handlers/get_app_name.h"

int dispatch(cmd_e ins, uint8_t p1, uint8_t p2, const buf_t *input) {
    switch (ins) {
        case GET_VERSION:
            return get_version(p1, p2, input);
        case GET_APP_NAME:
            return get_app_name(p1, p2, input);
        default:
            return send_sw(SW_INS_NOT_SUPPORTED);
    }
}
