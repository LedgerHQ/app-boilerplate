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

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t
#include <string.h>  // memmove

#include "cdc_mgmt.h"
#include "io.h"
#include "ble_ledger.h"
#include "usbd_ledger.h"
#include "nbgl_types.h"
#include "nbgl_front.h"

enum bletest_state_t {
    BLETEST_STATE_START_BRIDGE,
    BLETEST_STATE_START_PERIPHERAL,
    BLETEST_STATE_RUNNING_BRIDGE,
    BLETEST_STATE_RUNNING_PERIPHERAL,
    BLETEST_STATE_RESET,
    BLETEST_STATE_SHUTDOWN,
    BLETEST_STATE_GO_TO_BOOTLOADER,
    BLETEST_STATE_SEND_VERSION,
    BLETEST_STATE_PARSE_AT,
};

enum at_state_t {
    AT_STATE_IDLE,
    AT_STATE_NEED_MORE_DATA,
    AT_STATE_COMPLETE,
};

enum at_type_t {
    AT_TYPE_UNKOWN,
    AT_TYPE_QUERY,
    AT_TYPE_SET,
};

typedef struct {
    uint8_t offset;
    uint8_t length;
} at_cmd_parameter_t;

typedef struct {
    uint8_t state;  // at_state_t
    uint8_t buffer[128];
    uint16_t buffer_length;
    uint8_t cmd_offset;
    uint8_t cmd_length;
    uint8_t type;
    uint8_t nb_of_parameters;
    at_cmd_parameter_t parameter[10];
} at_cmd_t;

static const int8_t tx_power_table[32] = {-40, -21, -20, -19, -18, -17, -15, -14, -13, -12, -11,
                                          -10, -9,  -8,  -7,  -6,  -5,  -4,  -3,  255, -2,  255,
                                          -1,  255, 255, 0,   1,   2,   3,   4,   5,   6};

// General
// uint8_t                     exchange_buffer[MAX_APDU_SIZE+19];
static enum bletest_state_t bletest_state;
// static enum bletest_state_t backup_bletest_state;

// BLE related
static ble_cmd_data_t ble_cmd_data;

// HCI related
static uint8_t hci_cmd_sent;

// AT Related
static at_cmd_t at_cmd;

// CDC send related
static uint8_t cdc_send_buffer[128];
static uint16_t cdc_buffer_length;

// MISC
static uint32_t ticker_value;
static uint32_t logging_ticker_value;
static uint8_t logging_state;
static uint8_t auto_log_enabled;
static uint8_t auto_raw_rssi_available;
static uint8_t auto_raw_rssi_requested;
static int8_t tx_power_requested;

static int32_t os_str_to_int(char *s, uint8_t size) {
    uint8_t index = 0;
    int32_t n = 0;

    if (s[0] == '-') {
        index++;
    }

    for (index = 0U; index < size; index++) {
        if ((s[index] >= '0') && (s[index] <= '9')) {
            n *= 10;
            n += s[index] - '0';
        }
    }

    if (s[0] == '-') {
        return -n;
    } else {
        return n;
    }
}

static int8_t process_raw_rssi(uint8_t rssi_agc, int16_t rssi_val) {
    int32_t rssi;

    if ((rssi_val == 0U) || (rssi_agc > 0xbU)) {
        rssi = 127;
    } else {
        rssi = (int32_t) rssi_agc * 6 - 127;
        while (rssi_val > (int16_t) 30) {
            rssi += 6;
            rssi_val >>= 1;
        }

        rssi += (int32_t) (uint32_t) ((417U * rssi_val + 18080U) >> 10);
    }
    return (int8_t) rssi;
}

static void cdc_send(uint8_t *buffer, uint16_t length) {
    PRINTF((char *) buffer);
    if (length > sizeof(cdc_send_buffer)) {
        length = sizeof(cdc_send_buffer);
    }
    memcpy(cdc_send_buffer, buffer, length);
    cdc_buffer_length = length;
}

static void parse_at_cmd(uint8_t *buffer, uint16_t length, at_cmd_t *cmd) {
    uint8_t index = 0;

    for (index = 3; index < length; index++) {
        if ((buffer[index] == '\n') || (buffer[index] == '\r')) {
            cmd->state = AT_STATE_COMPLETE;
            break;
        }
        if ((!cmd->cmd_length) && (buffer[index] == '=')) {
            cmd->cmd_length = index - 3;
            cmd->nb_of_parameters = 0;
            cmd->parameter[cmd->nb_of_parameters].offset = index + 1;
            cmd->parameter[cmd->nb_of_parameters].length = 0;
            at_cmd.type = AT_TYPE_SET;
        } else if ((!cmd->cmd_length) && (buffer[index] == '?')) {
            cmd->cmd_length = index - cmd->cmd_offset;
            cmd->nb_of_parameters = 0;
            at_cmd.type = AT_TYPE_QUERY;
        } else if ((cmd->cmd_length) && (buffer[index] == ',')) {
            cmd->parameter[cmd->nb_of_parameters].length =
                index - cmd->parameter[cmd->nb_of_parameters].offset;
            cmd->nb_of_parameters++;
            cmd->parameter[cmd->nb_of_parameters].offset = index + 1;
            cmd->parameter[cmd->nb_of_parameters].length = 0;
        }
    }
    if (cmd->state == AT_STATE_COMPLETE) {
        if (!cmd->cmd_length) {
            cmd->cmd_length = index - cmd->cmd_offset;
        } else if ((cmd->parameter[cmd->nb_of_parameters].offset) &&
                   (!cmd->parameter[cmd->nb_of_parameters].length)) {
            cmd->parameter[cmd->nb_of_parameters].length =
                index - cmd->parameter[cmd->nb_of_parameters].offset;
            cmd->nb_of_parameters++;
        }
    }
}

static void process_at_cmd(uint8_t *buffer, uint16_t buffer_max_length, uint16_t length) {
    uint16_t rsp_start = 0;
    uint16_t rsp_offset = 0;
    uint8_t status = 0;

    parse_at_cmd(buffer, length, &at_cmd);

    if (at_cmd.state != AT_STATE_COMPLETE) {
        return;
    }
    at_cmd.state = AT_STATE_IDLE;
    ble_cmd_data.hci_cmd_buffer_length = 0;

    rsp_start = at_cmd.cmd_offset + at_cmd.cmd_length;
    rsp_offset = rsp_start;

    if (at_cmd.type == AT_TYPE_QUERY) {
        // QUERY
        rsp_start = 2;
        if (!memcmp(&buffer[at_cmd.cmd_offset], "VERSION", at_cmd.cmd_length)) {
            buffer[rsp_offset++] = ':';
            snprintf((char *) &buffer[rsp_offset],
                     buffer_max_length - rsp_offset,
                     "%d.%d.%d",
                     (uint8_t) MAJOR_VERSION,
                     (uint8_t) MINOR_VERSION,
                     (uint8_t) PATCH_VERSION);
            rsp_offset += strlen((char *) &buffer[rsp_offset]);
            buffer[rsp_offset++] = '\r';
            buffer[rsp_offset++] = '\n';
        } else if (!memcmp(&buffer[at_cmd.cmd_offset], "BLE_INIT", at_cmd.cmd_length)) {
            snprintf((char *) &buffer[rsp_offset],
                     buffer_max_length - rsp_offset,
                     ":%d\r\n",
                     (bletest_state == BLETEST_STATE_RUNNING_PERIPHERAL));
            rsp_offset += strlen((char *) &buffer[rsp_offset]);
        } else if (bletest_state == BLETEST_STATE_RUNNING_PERIPHERAL) {
            if (!memcmp(&buffer[at_cmd.cmd_offset], "BLE_RF_POWER", at_cmd.cmd_length)) {
                snprintf((char *) &buffer[rsp_offset],
                         buffer_max_length - rsp_offset,
                         ":%d\r\n",
                         BLE_LEDGER_requested_tx_power());
                rsp_offset += strlen((char *) &buffer[rsp_offset]);
            } else if (!memcmp(&buffer[at_cmd.cmd_offset], "BLE_LOG_CONN", at_cmd.cmd_length)) {
                snprintf((char *) &buffer[rsp_offset],
                         buffer_max_length - rsp_offset,
                         ":%d\r\n",
                         auto_log_enabled);
                rsp_offset += strlen((char *) &buffer[rsp_offset]);
            } else if (!memcmp(&buffer[at_cmd.cmd_offset], "BLE_ADV", at_cmd.cmd_length)) {
                snprintf((char *) &buffer[rsp_offset],
                         buffer_max_length - rsp_offset,
                         ":%d\r\n",
                         BLE_LEDGER_is_advertising_enabled());
                rsp_offset += strlen((char *) &buffer[rsp_offset]);
            } else {
                status = 1;
            }
        } else if (bletest_state == BLETEST_STATE_RUNNING_BRIDGE) {
            if (!memcmp(&buffer[at_cmd.cmd_offset], "BLE_RF_POWER", at_cmd.cmd_length)) {
                snprintf((char *) &buffer[rsp_offset],
                         buffer_max_length - rsp_offset,
                         ":%d\r\n",
                         tx_power_requested);
                rsp_offset += strlen((char *) &buffer[rsp_offset]);
            } else if (!memcmp(&buffer[at_cmd.cmd_offset], "BLE_LOG_RAW_RSSI", at_cmd.cmd_length)) {
                snprintf((char *) &buffer[rsp_offset],
                         buffer_max_length - rsp_offset,
                         ":%d\r\n",
                         (auto_raw_rssi_requested != 0));
                rsp_offset += strlen((char *) &buffer[rsp_offset]);
            } else {
                status = 1;
            }
        } else {
            status = 1;
        }
    }
    // SET
    else if ((at_cmd.type == AT_TYPE_SET) && (at_cmd.nb_of_parameters == 1)) {
        // SET with 1 parameter
        if (!memcmp(&buffer[at_cmd.cmd_offset], "BLE_INIT", at_cmd.cmd_length)) {
            PRINTF("BLE_INIT %d\n", bletest_state);
            if (os_str_to_int((char *) &buffer[at_cmd.parameter[0].offset],
                              at_cmd.parameter[0].length)) {
                if (bletest_state != BLETEST_STATE_RUNNING_PERIPHERAL) {
                    bletest_state = BLETEST_STATE_START_PERIPHERAL;
                    BLE_LEDGER_start(BLE_LEDGER_PROFILE_APDU);
                }
            } else {
                if (bletest_state != BLETEST_STATE_RUNNING_BRIDGE) {
                    bletest_state = BLETEST_STATE_START_BRIDGE;
                    BLE_LEDGER_start_bridge(cdc_mgmt_controller_packet_cb, cdc_mgmt_event_cb);
                }
            }
        } else if (bletest_state == BLETEST_STATE_RUNNING_PERIPHERAL) {
            if (!memcmp(&buffer[at_cmd.cmd_offset], "BLE_RF_POWER", at_cmd.cmd_length)) {
                status = BLE_LEDGER_set_tx_power(
                    1,
                    os_str_to_int((char *) &buffer[at_cmd.parameter[0].offset],
                                  at_cmd.parameter[0].length));
            } else if (!memcmp(&buffer[at_cmd.cmd_offset], "BLE_LOG_CONN", at_cmd.cmd_length)) {
                auto_log_enabled = os_str_to_int((char *) &buffer[at_cmd.parameter[0].offset],
                                                 at_cmd.parameter[0].length);
            } else if (!memcmp(&buffer[at_cmd.cmd_offset], "BLE_ADV", at_cmd.cmd_length)) {
                status = BLE_LEDGER_enable_advertising(
                    os_str_to_int((char *) &buffer[at_cmd.parameter[0].offset],
                                  at_cmd.parameter[0].length));
            } else if (!memcmp(&buffer[at_cmd.cmd_offset], "BLE_DISCONNECT", at_cmd.cmd_length)) {
                status = BLE_LEDGER_disconnect();
            } else if (!memcmp(&buffer[at_cmd.cmd_offset],
                               "BLE_PAIRING_CONFIRM",
                               at_cmd.cmd_length)) {
                BLE_LEDGER_confirm_numeric_comparison(
                    os_str_to_int((char *) &buffer[at_cmd.parameter[0].offset],
                                  at_cmd.parameter[0].length));
            } else if (!memcmp(&buffer[at_cmd.cmd_offset], "BLE_CLEAR_DB", at_cmd.cmd_length)) {
                BLE_LEDGER_clear_pairings();
            } else {
                status = 1;
            }
        } else if (bletest_state == BLETEST_STATE_RUNNING_BRIDGE) {
            if (!memcmp(&buffer[at_cmd.cmd_offset], "BLE_RF_POWER", at_cmd.cmd_length)) {
                uint8_t index = 0;
                int8_t value = (int8_t) os_str_to_int((char *) &buffer[at_cmd.parameter[0].offset],
                                                      at_cmd.parameter[0].length);
                for (index = 0; index < sizeof(tx_power_table); index++) {
                    if (tx_power_table[index] == value) {
                        break;
                    }
                }
                if (index < sizeof(tx_power_table)) {
                    ble_aci_hal_forge_cmd_set_tx_power_level(&ble_cmd_data, 1, index);
                    tx_power_requested = value;
                } else {
                    status = 1;
                }
            } else if (!memcmp(&buffer[at_cmd.cmd_offset],
                               "BLE_RECEIVER_TEST",
                               at_cmd.cmd_length)) {
                auto_raw_rssi_available = 1;
                ble_hci_forge_cmd_receiver_test(
                    &ble_cmd_data,
                    os_str_to_int((char *) &buffer[at_cmd.parameter[0].offset],
                                  at_cmd.parameter[0].length));
            } else if (!memcmp(&buffer[at_cmd.cmd_offset], "BLE_END_TEST", at_cmd.cmd_length)) {
                auto_raw_rssi_available = 0;
                ble_hci_forge_cmd_end_test(&ble_cmd_data);
            } else if (!memcmp(&buffer[at_cmd.cmd_offset], "BLE_TONE_STOP", at_cmd.cmd_length)) {
                ble_aci_forge_cmd_hal_tone_stop(&ble_cmd_data);
            } else if (!memcmp(&buffer[at_cmd.cmd_offset], "BLE_LOG_RAW_RSSI", at_cmd.cmd_length)) {
                auto_raw_rssi_requested =
                    (os_str_to_int((char *) &buffer[at_cmd.parameter[0].offset],
                                   at_cmd.parameter[0].length) != 0);
            } else {
                status = 1;
            }
        } else {
            status = 1;
        }
    } else if ((at_cmd.type == AT_TYPE_SET) && (at_cmd.nb_of_parameters == 2)) {
        // SET with 2 parameters
        if (bletest_state == BLETEST_STATE_RUNNING_PERIPHERAL) {
            if (!memcmp(&buffer[at_cmd.cmd_offset], "BLE_CONN_INT", at_cmd.cmd_length)) {
                status = BLE_LEDGER_update_connection_interval(
                    os_str_to_int((char *) &buffer[at_cmd.parameter[0].offset],
                                  at_cmd.parameter[0].length),
                    os_str_to_int((char *) &buffer[at_cmd.parameter[1].offset],
                                  at_cmd.parameter[1].length));
            } else {
                status = 1;
            }
        } else if (bletest_state == BLETEST_STATE_RUNNING_BRIDGE) {
            if (!memcmp(&buffer[at_cmd.cmd_offset], "BLE_TONE_START", at_cmd.cmd_length)) {
                ble_aci_forge_cmd_hal_tone_start(
                    &ble_cmd_data,
                    os_str_to_int((char *) &buffer[at_cmd.parameter[0].offset],
                                  at_cmd.parameter[0].length),
                    os_str_to_int((char *) &buffer[at_cmd.parameter[1].offset],
                                  at_cmd.parameter[1].length));
            } else {
                status = 1;
            }
        } else {
            status = 1;
        }
    } else {
        status = 1;
    }

    if ((status == 0) && (ble_cmd_data.hci_cmd_buffer_length)) {
        hci_cmd_sent = 1;
        memmove(&ble_cmd_data.hci_cmd_buffer[2],
                ble_cmd_data.hci_cmd_buffer,
                ble_cmd_data.hci_cmd_buffer_length);
        ble_cmd_data.hci_cmd_buffer[0] = 0x01;
        ble_cmd_data.hci_cmd_buffer[1] = ble_cmd_data.hci_cmd_buffer[3];
        ble_cmd_data.hci_cmd_buffer[2] = ble_cmd_data.hci_cmd_buffer[2];
        ble_cmd_data.hci_cmd_buffer[3] = ble_cmd_data.hci_cmd_buffer_length - 2;
        BLE_LEDGER_send_to_controller(ble_cmd_data.hci_cmd_buffer,
                                      ble_cmd_data.hci_cmd_buffer_length + 2);
    }

    if (!status) {
        snprintf((char *) &buffer[rsp_offset], buffer_max_length - rsp_offset, "OK\r\n");
    } else {
        snprintf((char *) &buffer[rsp_offset], buffer_max_length - rsp_offset, "ERROR\r\n");
    }
    rsp_offset += strlen((char *) &buffer[rsp_offset]);
    buffer[rsp_offset] = 0;
    io_send_raw_response(&buffer[rsp_start], rsp_offset - rsp_start);
}

void cdc_mgmt_event_cb(uint32_t event, uint32_t param) {
    char buffer[128];
    uint8_t buffer_length;
    ble_connection_t connection;

    PRINTF("cdc_mgmt_event_cb %08X %d %d\n", event, param, bletest_state);

    switch (event) {
        case 0x00000000:
            if ((bletest_state == BLETEST_STATE_START_BRIDGE) && (param == 0)) {
                hci_cmd_sent = 0;
                bletest_state = BLETEST_STATE_RUNNING_BRIDGE;
                cdc_send((uint8_t *) "<BLE_EVT_START_BRIDGE=1\r\n", 25);
            } else if ((bletest_state == BLETEST_STATE_START_PERIPHERAL) && (param == 1)) {
                bletest_state = BLETEST_STATE_RUNNING_PERIPHERAL;
                cdc_send((uint8_t *) "<BLE_EVT_START_PERIPHERAL=1\r\n", 29);
            }
            break;

        case 0x00000001:
            BLE_LEDGER_get_connection_info(&connection);
            snprintf(buffer,
                     sizeof(buffer),
                     "<BLE_EVT_CONN=%d.%d,%d,%d\r\n",
                     (int32_t) (connection.conn_interval * 125) / 100,
                     (int32_t) (connection.conn_interval * 125) % 100,
                     connection.conn_latency,
                     (int32_t) (connection.supervision_timeout) * 10);
            buffer_length = strlen(buffer);
            cdc_send((uint8_t *) buffer, buffer_length);
            break;

        case 0x00000002:
            BLE_LEDGER_get_connection_info(&connection);
            snprintf(buffer,
                     sizeof(buffer),
                     "<BLE_EVT_UPD_CONN=%d.%d,%d,%d\r\n",
                     (int32_t) (connection.conn_interval * 125) / 100,
                     (int32_t) (connection.conn_interval * 125) % 100,
                     connection.conn_latency,
                     (int32_t) (connection.supervision_timeout) * 10);
            buffer_length = strlen(buffer);
            cdc_send((uint8_t *) buffer, buffer_length);
            break;

        case 0x00000003:
            cdc_send((uint8_t *) "<BLE_EVT_DISCONN=1\r\n", 20);
            break;

        case 0x00000010:
            if (param) {
                cdc_send((uint8_t *) "<BLE_EVT_ENCRYPTED=1\r\n", 22);
            } else {
                cdc_send((uint8_t *) "<BLE_EVT_ENCRYPTED=0\r\n", 22);
            }
            break;

        case 0x00000020:
            cdc_send((uint8_t *) "<BLE_EVT_PAIRING=OK,0\r\n", 21);
            break;

        case 0x00000021:
            cdc_send((uint8_t *) "<BLE_EVT_PAIRING=TIMEOUT,0\r\n", 26);
            break;

        case 0x00000022:
            snprintf(buffer, sizeof(buffer), "<BLE_EVT_PAIRING=FAILED, %d\r\n", param);
            buffer_length = strlen(buffer);
            cdc_send((uint8_t *) buffer, buffer_length);
            break;

        case 0x00000030:
            snprintf(buffer, sizeof(buffer), "<BLE_EVT_VALUE_CONFIRM=%06d\r\n", param);
            buffer_length = strlen(buffer);
            cdc_send((uint8_t *) buffer, buffer_length);
            break;

        case 0x000000A0: {
            BLE_LEDGER_get_connection_info(&connection);

            if (connection.connection_handle != 0xFFFF) {
                snprintf(buffer,
                         sizeof(buffer),
                         "<BLE_CONN_INFO=%d,%d,%d,%d,%dM,%dM,%d.%d\r\n",
                         (int32_t) connection.rssi_level,
                         (int32_t) connection.current_transmit_power_level,
                         (int32_t) connection.max_transmit_power_level,
                         (int32_t) connection.requested_tx_power,
                         (int32_t) connection.rx_phy,
                         (int32_t) connection.tx_phy,
                         (int32_t) (connection.conn_interval * 125) / 100,
                         (int32_t) (connection.conn_interval * 125) % 100);
                buffer_length = strlen(buffer);
            } else {
                snprintf(buffer,
                         sizeof(buffer),
                         "<BLE_CONN_INFO=N/A,N/A,N/A,%d,N/A,N/A,N/A\r\n",
                         (int32_t) connection.requested_tx_power);
                buffer_length = strlen(buffer);
            }
            cdc_send((uint8_t *) buffer, buffer_length);
            break;
        }

        default:
            break;
    }
}

void cdc_mgmt_controller_packet_cb(uint8_t *packet, uint16_t length) {
    /*
            int index = 0 ;
            PRINTF("CDC Tx [%d] :", length);
            for (index = 0; index < length; index++) {
                    PRINTF(" %02X", packet[index]);
            }
            PRINTF("\n");
    */
    if (!hci_cmd_sent) {
        USBD_LEDGER_send(USBD_LEDGER_CLASS_CDC_DATA, packet, length, 0);
    } else {
        char buffer[128];
        if (packet[6] == 0) {
            hci_cmd_sent = 0;
            if ((packet[4] == 0x1F) && (packet[5] == 0x20)) {
                snprintf(buffer, sizeof(buffer), "<BLE_RX_PKT_NB=%d\r\n", U2LE(packet, 7));
                length = strlen(buffer);
                USBD_LEDGER_send(USBD_LEDGER_CLASS_CDC_DATA, (uint8_t *) buffer, length, 0);
            } else if ((packet[4] == 0x32) && (packet[5] == 0xFC)) {
                int8_t raw_rssi = process_raw_rssi(packet[9], (int16_t) U2LE(packet, 7));
                snprintf(buffer, sizeof(buffer), "<BLE_RAW_RSSI=%d\r\n", (int32_t) raw_rssi);
                length = strlen(buffer);
                USBD_LEDGER_send(USBD_LEDGER_CLASS_CDC_DATA, (uint8_t *) buffer, length, 0);
            }
        }
    }
}

int cdc_mgmt_process_req(uint8_t *buffer, uint16_t length) {
    if (!length) {
        return 0;
    }
    /*
            int index = 0 ;
            PRINTF("CDC Rx [%d] :", length);
            for (index = 0; index < length; index++) {
                    PRINTF(" %02X", buffer[index]);
            }
            PRINTF("\n");
    */
    switch (buffer[0]) {
        case 0x01:  // CMD
            BLE_LEDGER_send_to_controller(buffer, length);
            break;

        case 0x02:  // ACL
            BLE_LEDGER_send_to_controller(buffer, length);
            break;

        case 0x10:  // SYS CMD
            BLE_LEDGER_send_to_controller(buffer, length);
            break;

        case 0x20:  // LOCAL CMD
            BLE_LEDGER_send_to_controller(buffer, length);
            break;

        default:
            if ((buffer[0] == 'A') && (buffer[1] == 'T') && (buffer[2] == '+')) {
                if (length < sizeof(at_cmd.buffer)) {
                    at_cmd.cmd_offset = 3;
                    at_cmd.cmd_length = 0;
                    at_cmd.type = AT_TYPE_UNKOWN;
                    at_cmd.nb_of_parameters = 0;
                    at_cmd.state = AT_STATE_NEED_MORE_DATA;
                    memcpy(at_cmd.buffer, buffer, length);
                    at_cmd.buffer_length = length;
                    process_at_cmd(at_cmd.buffer, sizeof(at_cmd.buffer), at_cmd.buffer_length);
                }
            } else if (at_cmd.state == AT_STATE_NEED_MORE_DATA) {
                if (at_cmd.buffer_length + length < sizeof(at_cmd.buffer)) {
                    memcpy(&at_cmd.buffer[at_cmd.buffer_length], buffer, length);
                    at_cmd.buffer_length += length;
                    process_at_cmd(at_cmd.buffer, sizeof(at_cmd.buffer), at_cmd.buffer_length);
                } else {
                    at_cmd.state = AT_STATE_IDLE;
                    at_cmd.buffer_length = 0;
                }
            }
            break;
    }

    return 0;
}

void cdc_mgmt_init(void) {
    ticker_value = 0;
    logging_state = 0;
    logging_ticker_value = 0;
    cdc_buffer_length = 0;
    bletest_state = BLETEST_STATE_START_BRIDGE;
    BLE_LEDGER_init();
    BLE_LEDGER_start_bridge(cdc_mgmt_controller_packet_cb, cdc_mgmt_event_cb);
}

static uint8_t count = 0;
static uint8_t color_index = 0;

void nbgl_fullScreenClear(color_t color, bool refresh) {
    // Draw full screen
    nbgl_area_t area = {
        .x0 = 0,
        .y0 = 0,
        .width = SCREEN_WIDTH,
        .height = SCREEN_HEIGHT,
        // .bpp = NBGL_BPP_1,
        .backgroundColor = color,
    };
    nbgl_frontDrawRect(&area);
    if (refresh) {
        nbgl_frontRefreshArea(&area, FULL_COLOR_REFRESH, POST_REFRESH_FORCE_POWER_OFF);
    }
}

#ifndef TARGET_APEX
static const color_t COLORS[] = {
    BLACK,
    DARK_GRAY,
    LIGHT_GRAY,
    WHITE,
};
#else
static const color_t COLORS[] = {
    BLACK,
    WHITE,
};
#endif

void cdc_mgmt_tick(void) {
    const uint32_t nb_colors = sizeof(COLORS) / sizeof(color_t);
    ticker_value += 100;
    if (count % 10 == 0) {
        count = 0;
        // display sth
        nbgl_fullScreenClear(COLORS[color_index], true);
        color_index++;
        if (color_index % nb_colors == 0) {
            color_index = 0;
        }
    }
    count++;
    if (cdc_buffer_length) {
        PRINTF("cdc_mgmt_tick %d\n", cdc_buffer_length);
        USBD_LEDGER_send(USBD_LEDGER_CLASS_CDC_DATA, cdc_send_buffer, cdc_buffer_length, 0);
        cdc_buffer_length = 0;
    } else if (bletest_state == BLETEST_STATE_RUNNING_BRIDGE) {
        if ((logging_state == 0) && (ticker_value >= (logging_ticker_value + 100))) {
            if (auto_raw_rssi_requested && auto_raw_rssi_available) {
                hci_cmd_sent = 1;
                ble_aci_hal_forge_cmd_read_raw_rssi(&ble_cmd_data);
                memmove(&ble_cmd_data.hci_cmd_buffer[2],
                        ble_cmd_data.hci_cmd_buffer,
                        ble_cmd_data.hci_cmd_buffer_length);
                ble_cmd_data.hci_cmd_buffer[0] = 0x01;
                ble_cmd_data.hci_cmd_buffer[1] = ble_cmd_data.hci_cmd_buffer[3];
                ble_cmd_data.hci_cmd_buffer[2] = ble_cmd_data.hci_cmd_buffer[2];
                ble_cmd_data.hci_cmd_buffer[3] = ble_cmd_data.hci_cmd_buffer_length - 2;
                BLE_LEDGER_send_to_controller(ble_cmd_data.hci_cmd_buffer,
                                              ble_cmd_data.hci_cmd_buffer_length + 2);
            }
            logging_state++;
        } else if ((logging_state > 0) && (ticker_value >= (logging_ticker_value + 500))) {
            logging_state = 0;
            logging_ticker_value = ticker_value;
        }
    } else if (bletest_state == BLETEST_STATE_RUNNING_PERIPHERAL) {
        if ((logging_state == 0) && (ticker_value >= (logging_ticker_value + 100))) {
            BLE_LEDGER_trig_read_rssi();
            logging_state++;
        } else if ((logging_state == 1) && (ticker_value >= (logging_ticker_value + 200))) {
            logging_state++;
        } else if ((logging_state == 2) && (ticker_value >= (logging_ticker_value + 300))) {
            BLE_LEDGER_trig_read_transmit_power_level(1);
            logging_state++;
        } else if ((logging_state == 3) && (ticker_value >= (logging_ticker_value + 400))) {
            BLE_LEDGER_trig_read_transmit_power_level(0);
            logging_state++;
        } else if ((logging_state > 3) && (ticker_value >= (logging_ticker_value + 500))) {
            logging_state = 0;
            logging_ticker_value = ticker_value;
            if (auto_log_enabled) {
                cdc_mgmt_event_cb(0x000000A0, 0);
            }
        }
    }
}
