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
#include <stdbool.h>

#include "dispatcher.h"
#include "../constants.h"
#include "../globals.h"
#include "../types.h"
#include "../io.h"
#include "../sw.h"
#include "../common/buffer.h"
#include "../handler/get_version.h"
#include "../handler/get_app_name.h"
#include "../handler/get_public_key.h"
#include "../handler/sign_tx.h"

extern uint8_t touch_debug[NB_TOUCH_DEBUG*TOUCH_DEBUG_LEN];
extern uint16_t point_idx;
extern uint8_t last_touch_state;
extern uint16_t G_ticks;

int apdu_dispatcher(const command_t *cmd) {
    if (cmd->cla != CLA) {
        return io_send_sw(SW_CLA_NOT_SUPPORTED);
    }

    buffer_t buf = {0};
    uint8_t sensi_buffer[224];

    switch (cmd->ins) {
        case GET_VERSION:
            if (cmd->p1 != 0 || cmd->p2 != 0) {
                return io_send_sw(SW_WRONG_P1P2);
            }

            return handler_get_version();
        case GET_APP_NAME:
            if (cmd->p1 != 0 || cmd->p2 != 0) {
                return io_send_sw(SW_WRONG_P1P2);
            }

            return handler_get_app_name();
        case GET_PUBLIC_KEY:
            if (cmd->p1 > 1 || cmd->p2 > 0) {
                return io_send_sw(SW_WRONG_P1P2);
            }

            if (!cmd->data) {
                return io_send_sw(SW_WRONG_DATA_LENGTH);
            }

            buf.ptr = cmd->data;
            buf.size = cmd->lc;
            buf.offset = 0;

            return handler_get_public_key(&buf, (bool) cmd->p1);
        case SIGN_TX:
            if ((cmd->p1 == P1_START && cmd->p2 != P2_MORE) ||  //
                cmd->p1 > P1_MAX ||                             //
                (cmd->p2 != P2_LAST && cmd->p2 != P2_MORE)) {
                return io_send_sw(SW_WRONG_P1P2);
            }

            if (!cmd->data) {
                return io_send_sw(SW_WRONG_DATA_LENGTH);
            }

            buf.ptr = cmd->data;
            buf.size = cmd->lc;
            buf.offset = 0;

            return handler_sign_tx(&buf, cmd->p1, (bool) (cmd->p2 & P2_MORE));
        case TOUCH_DEBUG_GET:
            buf.ptr = touch_debug;
            buf.size = sizeof(touch_debug);
            buf.offset = 0;
            int ret = io_send_response(&buf, SW_OK);
            memset(touch_debug, 0xFF, sizeof(touch_debug));
            point_idx = 0;
            return ret;
        case TOUCH_DEBUG_CLEAR:
            memset(touch_debug, 0xFF, sizeof(touch_debug));
            point_idx = 0;
            G_ticks = 0;
            return io_send_sw(SW_OK);
        case TOUCH_DEBUG_READ_SENSI:
            io_seproxyhal_touch_read_sensi(sensi_buffer);
            buf.ptr = sensi_buffer;
            buf.size = sizeof(sensi_buffer);
            buf.offset = 0;
            return io_send_response(&buf, SW_OK);
        default:
            return io_send_sw(SW_INS_NOT_SUPPORTED);
    }
}
