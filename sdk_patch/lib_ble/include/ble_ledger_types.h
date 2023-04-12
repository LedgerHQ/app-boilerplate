/* @BANNER@ */

#pragma once

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "ble_types.h"
#include "ble_cmd.h"

/* Exported enumerations -----------------------------------------------------*/
typedef enum {
	BLE_PROFILE_STATUS_OK                 = 0x00,
	BLE_PROFILE_STATUS_OK_AND_SEND_PACKET = 0x01,
	BLE_PROFILE_STATUS_OK_PROCEDURE_END   = 0x02,
	BLE_PROFILE_STATUS_BAD_PARAMETERS     = 0xFF,
} ble_profile_status_e;

/* Exported defines   --------------------------------------------------------*/

/* Exported types, structures, unions ----------------------------------------*/
typedef struct {
	uint16_t connection_handle;
	uint8_t  role_slave;
	uint8_t  peer_address_random;
	uint8_t  peer_address[6];
	uint16_t conn_interval;
	uint16_t conn_latency;
	uint16_t supervision_timeout;
	uint8_t  master_clock_accuracy;
	uint8_t  tx_phy;
	uint8_t  rx_phy;
	uint16_t max_tx_octets;
	uint16_t max_tx_time;
	uint16_t max_rx_octets;
	uint16_t max_rx_time;
	uint8_t  encrypted;
	int8_t   rssi_level;
	int8_t   current_transmit_power_level;
	int8_t   max_transmit_power_level;
	int8_t   requested_tx_power;
} ble_connection_t;

typedef void    (*ble_profile_init_t) (ble_cmd_data_t *cmd_data,
                                       void           *cookie);
typedef uint8_t (*ble_profile_create_db_t) (uint8_t  *hci_buffer,
                                            uint16_t length,
                                            void     *cookie);
typedef uint8_t (*ble_profile_handle_in_range_t) (uint16_t gatt_handle,
                                                  void     *cookie);

typedef void (*ble_profile_connection_evt_t) (ble_connection_t *connection,
                                              void             *cookie);
typedef void (*ble_profile_connection_update_evt_t) (ble_connection_t *connection,
                                                     void             *cookie);
typedef void (*ble_profile_encryption_changed_t) (uint8_t encrypted,
                                                  void    *cookie);

typedef uint8_t (*ble_profile_att_modified_t) (uint8_t  *hci_buffer,
                                               uint16_t length,
                                               void     *cookie);
typedef uint8_t (*ble_profile_write_permit_req_t) (uint8_t  *hci_buffer,
                                                   uint16_t length,
                                                   void     *cookie);
typedef uint8_t (*ble_profile_mtu_changed_t) (uint16_t mtu,
                                              void     *cookie);

typedef uint8_t (*ble_profile_write_rsp_ack_t) (void *cookie);
typedef uint8_t (*ble_profile_update_char_value_ack_t) (void *cookie);

typedef uint8_t (*ble_profile_send_packet_t) (uint8_t  *packet,
                                              uint16_t length,
                                              void     *cookie);

typedef struct ble_profile_info_t_{
	uint8_t type; // ble_ledger_profile_mask_e

	ble_uuid_t service_uuid;

	ble_profile_init_t          init;
	ble_profile_create_db_t     create_db;
	ble_profile_handle_in_range_t handle_in_range;

	ble_profile_connection_evt_t        connection_evt;
	ble_profile_connection_update_evt_t connection_update_evt;
	ble_profile_encryption_changed_t    encryption_changed;

	ble_profile_att_modified_t     att_modified;
	ble_profile_write_permit_req_t write_permit_req;
	ble_profile_mtu_changed_t      mtu_changed;

	ble_profile_write_rsp_ack_t         write_rsp_ack;
	ble_profile_update_char_value_ack_t update_char_val_ack;

	ble_profile_send_packet_t send_packet;

	void *cookie;
} ble_profile_info_t;

/* Exported macros------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/

/* Exported functions prototypes--------------------------------------------- */

