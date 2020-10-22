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

#include "os.h"
#include "ux.h"

#include "globals.h"

uint32_t output_len = 0;

void io_seproxyhal_display(const bagl_element_t *element) {
    io_seproxyhal_display_default((bagl_element_t *) element);
}

uint8_t io_event(uint8_t channel) {
    switch (G_io_seproxyhal_spi_buffer[0]) {
        case SEPROXYHAL_TAG_FINGER_EVENT:
            UX_FINGER_EVENT(G_io_seproxyhal_spi_buffer);
            break;
        case SEPROXYHAL_TAG_BUTTON_PUSH_EVENT:
            UX_BUTTON_PUSH_EVENT(G_io_seproxyhal_spi_buffer);
            break;
        case SEPROXYHAL_TAG_STATUS_EVENT:
            if (G_io_apdu_media == IO_APDU_MEDIA_USB_HID &&  //
                !(U4BE(G_io_seproxyhal_spi_buffer, 3) &      //
                  SEPROXYHAL_TAG_STATUS_EVENT_FLAG_USB_POWERED)) {
                THROW(EXCEPTION_IO_RESET);
            }
            /* fallthrough */
        default:
            UX_DEFAULT_EVENT();
            break;
        case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
            UX_DISPLAYED_EVENT({});
            break;
        case SEPROXYHAL_TAG_TICKER_EVENT:
            UX_TICKER_EVENT(G_io_seproxyhal_spi_buffer, {});
            break;
    }
    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }

    return 1;
}

uint16_t io_exchange_al(uint8_t channel, uint16_t tx_len) {
    switch (channel & ~(IO_FLAGS)) {
        case CHANNEL_KEYBOARD:
            break;
        case CHANNEL_SPI:
            if (tx_len) {
                io_seproxyhal_spi_send(G_io_apdu_buffer, tx_len);

                if (channel & IO_RESET_AFTER_REPLIED) {
                    reset();
                }

                return 0;
            } else {
                return io_seproxyhal_spi_recv(G_io_apdu_buffer, sizeof(G_io_apdu_buffer), 0);
            }
        default:
            THROW(INVALID_PARAMETER);
    }

    return 0;
}

int recv() {
    int ret;

    switch (io_state) {
        case READY:
            io_state = RECEIVED;
            ret = io_exchange(CHANNEL_APDU, output_len);
            break;
        case RECEIVED:
            io_state = WAITING;
            ret = io_exchange(CHANNEL_APDU | IO_ASYNCH_REPLY, output_len);
            io_state = RECEIVED;
            break;
        case WAITING:
            io_state = READY;
            ret = -1;
            break;
    }

    return ret;
}

int send(const buf_t *buf, uint16_t status_word) {
    int ret;
    output_len = 0;

    if (buf != NULL) {
        os_memmove(G_io_apdu_buffer, buf->bytes, buf->size);
        output_len = buf->size;
    }
    G_io_apdu_buffer[output_len++] = (uint8_t)(status_word >> 8);
    G_io_apdu_buffer[output_len++] = (uint8_t)(status_word & 0xFF);

    PRINTF("<= %.*H\n", buf->size, buf->bytes);

    switch (io_state) {
        case READY:
            ret = -1;
            break;
        case RECEIVED:
            io_state = READY;
            ret = 0;
            break;
        case WAITING:
            ret = io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, output_len);
            output_len = 0;
            io_state = READY;
            break;
    }

    return ret;
}

int send_sw(uint16_t status_word) {
    return send(NULL, status_word);
}
