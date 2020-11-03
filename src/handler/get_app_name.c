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

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "get_app_name.h"
#include "../globals.h"
#include "../io.h"
#include "../sw.h"
#include "../types.h"

int set_app_name(uint8_t *out, size_t out_len, char *name, size_t name_len) {
    if (out_len < name_len) {
        return -1;
    }

    strncpy((char *) out, name, name_len);

    return 0;
}

int get_app_name(uint8_t *cdata, uint8_t cdata_len) {
    _Static_assert(APPNAME_LEN < MAX_APPNAME_LEN, "APPNAME must be at most 64 characters!");

    response_t resp = {.data = (uint8_t[APPNAME_LEN]){0}, .data_len = APPNAME_LEN};

    if (set_app_name(resp.data, resp.data_len, APPNAME, APPNAME_LEN) < 0) {
        return send_sw(SW_WRONG_RESPONSE_LENGTH);
    }

    return send_response(&resp, SW_OK);
}
