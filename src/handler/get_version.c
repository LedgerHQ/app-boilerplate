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
#include <stdlib.h>
#include <limits.h>
#include <assert.h>

#include "get_version.h"
#include "../globals.h"
#include "../io.h"
#include "../sw.h"
#include "../types.h"

int set_app_version(uint8_t *out, size_t out_len, uint8_t major, uint8_t minor, uint8_t patch) {
    if (out_len < 3) {
        return -1;
    }

    out[0] = major;
    out[1] = minor;
    out[2] = patch;

    return 0;
}

int get_version(uint8_t *cdata, uint8_t cdata_len) {
    _Static_assert(APPVERSION_LEN == 3, "Length of (MAJOR || MINOR || PATCH) must be 3!");
    _Static_assert(MAJOR_VERSION >= 0 && MAJOR_VERSION <= UINT8_MAX,
                   "MAJOR version must be between 0 and 255!");
    _Static_assert(MINOR_VERSION >= 0 && MINOR_VERSION <= UINT8_MAX,
                   "MINOR version must be between 0 and 255!");
    _Static_assert(PATCH_VERSION >= 0 && PATCH_VERSION <= UINT8_MAX,
                   "PATCH version must be between 0 and 255!");

    response_t resp = {.data = (uint8_t[APPVERSION_LEN]){0}, .data_len = APPVERSION_LEN};

    if (set_app_version(resp.data,      //
                        resp.data_len,  //
                        MAJOR_VERSION,  //
                        MINOR_VERSION,  //
                        PATCH_VERSION) < 0) {
        return send_sw(SW_WRONG_RESPONSE_LENGTH);
    }

    return send_response(&resp, SW_OK);
}
