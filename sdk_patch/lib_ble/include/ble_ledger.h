/* @BANNER@ */

#pragma once

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "os_id.h"
#include "lcx_crc.h"
#include "ble_types.h"
#include "ble_ledger_types.h"

/* Exported enumerations -----------------------------------------------------*/
typedef enum {
	BLE_LEDGER_PROFILE_APDU   = 0x0001,
	BLE_LEDGER_PROFILE_U2F    = 0x0004,
} ble_ledger_profile_mask_e;

/* Exported defines   --------------------------------------------------------*/
#define LEDGER_BLE_get_mac_address(address) {  \
	unsigned char se_serial[8] = {0};          \
	os_serial(se_serial, sizeof(se_serial));   \
	unsigned int uid = cx_crc16(se_serial, 4); \
	address[0] = uid;                          \
	address[1] = uid>>8;                       \
	uid = cx_crc16(se_serial+4, 4);            \
	address[2] = uid;                          \
	address[3] = uid>>8;                       \
	address[4] = 0xF3;                         \
	address[5] = 0xDE;                         \
}

/* Exported types, structures, unions ----------------------------------------*/
typedef void (*cdc_controller_packet_cb_t) (uint8_t  *packet,
                                            uint16_t length);

typedef void (*cdc_event_cb_t) (uint32_t event, uint32_t param);

/* Exported macros------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/
extern uint8_t BLE_LEDGER_apdu_buffer[IO_APDU_BUFFER_SIZE];

/* Exported functions prototypes--------------------------------------------- */
void    BLE_LEDGER_init(void);
void    BLE_LEDGER_start(uint16_t profile_mask); // mask forged with ble_ledger_profile_mask_e
void    BLE_LEDGER_swith_to_bridge(void);
uint8_t BLE_LEDGER_enable_advertising(uint8_t enable);
void    BLE_LEDGER_reset_pairings(void);

// Rx
uint32_t BLE_LEDGER_receive(uint8_t* buffer, uint16_t buffer_length);

// Tx
uint32_t BLE_LEDGER_send(uint8_t* packet, uint16_t packet_length, uint32_t timeout_ms);

void BLE_LEDGER_start_bridge(cdc_controller_packet_cb_t cdc_controller_packet_cb,
                             cdc_event_cb_t             cdc_event_cb);
void BLE_LEDGER_send_to_controller(uint8_t* packet, uint16_t packet_length);


uint8_t BLE_LEDGER_set_tx_power(uint8_t high_power, int8_t value);
uint8_t BLE_LEDGER_requested_tx_power(void);

uint8_t BLE_LEDGER_is_advertising_enabled(void);

uint8_t BLE_LEDGER_disconnect(void);

void BLE_LEDGER_confirm_numeric_comparison(uint8_t confirm);

void BLE_LEDGER_clear_pairings(void);

void BLE_LEDGER_get_connection_info(ble_connection_t *connection_info);

void BLE_LEDGER_trig_read_rssi(void);
void BLE_LEDGER_trig_read_transmit_power_level(uint8_t current);

uint8_t BLE_LEDGER_update_connection_interval(uint16_t conn_interval_min,
                                              uint16_t conn_interval_max);
