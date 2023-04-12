/* @BANNER@ */

/* Includes ------------------------------------------------------------------*/
#include "os.h"
#include "os_settings.h"
#include "os_io_seproxyhal.h"

#include <string.h>

#include "lcx_rng.h"

#include "ble_cmd.h"
#include "ble_ledger.h"
#include "ble_ledger_profile_apdu.h"

#pragma GCC diagnostic ignored "-Wcast-qual"

/* Private enumerations ------------------------------------------------------*/
typedef enum {
	BLE_STATE_INITIALIZING,
	BLE_STATE_INITIALIZED,
	BLE_STATE_CONFIGURE_ADVERTISING,
	BLE_STATE_CONNECTED,
	BLE_STATE_DISCONNECTING,
	BLE_STATE_SWITCH_TO_BRIDGE,
	BLE_STATE_STARTING_BRIDGE,
	BLE_STATE_BRIDGING,
} ble_state_t;

typedef enum {
	BLE_INIT_STEP_IDLE,
	BLE_INIT_STEP_RESET,
	BLE_INIT_STEP_STATIC_ADDRESS,
	BLE_INIT_STEP_GATT_INIT,
	BLE_INIT_STEP_GAP_INIT,
	BLE_INIT_STEP_SET_IO_CAPABILITIES,
	BLE_INIT_STEP_SET_AUTH_REQUIREMENTS,
	BLE_INIT_STEP_PROFILE_INIT,
	BLE_INIT_STEP_PROFILE_CREATE_DB,
	BLE_INIT_STEP_SET_TX_POWER_LEVEL,
	BLE_INIT_STEP_CONFIGURE_ADVERTISING,
	BLE_INIT_STEP_END,
} ble_init_step_t;

typedef enum {
	BLE_CONFIG_ADV_STEP_IDLE,
	BLE_CONFIG_ADV_STEP_SET_ADV_DATAS,
	BLE_CONFIG_ADV_STEP_SET_SCAN_RSP_DATAS,
	BLE_CONFIG_ADV_STEP_SET_GAP_DEVICE_NAME,
	BLE_CONFIG_ADV_STEP_START,
	BLE_CONFIG_ADV_STEP_END,
} ble_config_adv_step_t;

/* Private defines------------------------------------------------------------*/
#define BLE_SLAVE_CONN_INTERVAL_MIN 12  // 15ms
#define BLE_SLAVE_CONN_INTERVAL_MAX 24  // 30ms

#define BLE_ADVERTISING_INTERVAL_MIN 48 // 30ms
#define BLE_ADVERTISING_INTERVAL_MAX 96 // 60ms

/* Private types, structures, unions -----------------------------------------*/

typedef struct {
	// General
	ble_state_t         state;
	char                device_name[BLE_GAP_MAX_LOCAL_NAME_LENGTH+1];
	char                device_name_length;
	uint8_t             random_address[BLE_CONFIG_DATA_RANDOM_ADDRESS_LEN];
	uint8_t             nb_of_profile;
	ble_profile_info_t* profile[2];
	uint16_t            profiles;

	// Init
	ble_init_step_t init_step;

	// Advertising configuration
	ble_config_adv_step_t adv_step;
	uint8_t               adv_enable;

	// HCI
	ble_cmd_data_t cmd_data;
	uint8_t        hci_reading_current_tx_power;

	// GAP
	uint16_t         gap_service_handle;
	uint16_t         gap_device_name_characteristic_handle;
	uint16_t         gap_appearance_characteristic_handle;
	uint8_t          advertising_enabled;
	ble_connection_t connection;
	uint16_t         pairing_code;
	uint8_t          pairing_in_progress;
	uint8_t          adv_tx_power;

	// PAIRING
	uint8_t clear_pairing;

	// APDU
	uint16_t apdu_buffer_length;
	uint8_t  apdu_buffer[IO_APDU_BUFFER_SIZE];

	// BRIDGE mode
	cdc_controller_packet_cb_t cdc_controller_packet_cb;
	cdc_event_cb_t             cdc_event_cb;

} ble_ledger_data_t;

#ifdef HAVE_PRINTF
#define DEBUG PRINTF
//#define DEBUG(...)
#else // !HAVE_PRINTF
#define DEBUG(...)
#endif // !HAVE_PRINTF

/* Private macros-------------------------------------------------------------*/

/* Private functions prototypes ----------------------------------------------*/
// Init
static void get_device_name(void);
static void init_mngr(uint8_t *hci_buffer, uint16_t length);

// Advertising
static void configure_advertising_mngr(uint16_t opcode);
static void advertising_enable(uint8_t enable);
static void start_advertising(void);

// Pairing UX
//static void ask_user_pairing_numeric_comparison(uint32_t code);
//static void rsp_user_pairing_numeric_comparison(unsigned int status);
static void ask_user_pairing_passkey(void);
static void rsp_user_pairing_passkey(unsigned int status);
static void end_pairing_ux(void);

// HCI
static void hci_evt_cmd_complete(uint8_t *buffer, uint16_t length);
static void hci_evt_le_meta_evt (uint8_t *buffer, uint16_t length);
static void hci_evt_vendor      (uint8_t *buffer, uint16_t length);

static uint32_t send_hci_packet(uint32_t timeout_ms);

/* Exported variables --------------------------------------------------------*/
uint8_t BLE_LEDGER_apdu_buffer[IO_APDU_BUFFER_SIZE];

/* Private variables ---------------------------------------------------------*/
static ble_ledger_data_t ble_ledger_data;

static const int8_t tx_power_table[32] = {
	-40, -21, -20, -19, -18, -17, -15, -14,
	-13, -12, -11, -10,  -9,  -8,  -7,  -6,
	 -5,  -4,  -3, 255,  -2, 255,  -1, 255,
	255,   0,   1,   2,   3,   4,   5,   6
};

/* Private functions ---------------------------------------------------------*/
static void get_device_name(void)
{
	memset(ble_ledger_data.device_name, 0,
	       sizeof(ble_ledger_data.device_name));
	ble_ledger_data.device_name_length = os_setting_get(OS_SETTING_DEVICENAME,
	                                                    (uint8_t*)ble_ledger_data.device_name,
	                                                    sizeof(ble_ledger_data.device_name)-1);
}

static void init_mngr(uint8_t *hci_buffer, uint16_t length)
{
	uint16_t opcode = 0;

	if (hci_buffer) {
		opcode = U2LE(hci_buffer, 1);
	}

	if (  (ble_ledger_data.cmd_data.hci_cmd_opcode != 0xFFFF)
	    &&(opcode != ble_ledger_data.cmd_data.hci_cmd_opcode)
	   ) {
		// Unexpected event => BLE_TODO
		return;
	}

	if (ble_ledger_data.init_step == BLE_INIT_STEP_IDLE) {
		DEBUG("INIT START\n");
	}
	else if (  (length >= 6)
	         &&(ble_ledger_data.init_step == BLE_INIT_STEP_GAP_INIT)
	        ) {
		ble_ledger_data.gap_service_handle                    = U2LE(hci_buffer, 4);
		ble_ledger_data.gap_device_name_characteristic_handle = U2LE(hci_buffer, 6);
		ble_ledger_data.gap_appearance_characteristic_handle  = U2LE(hci_buffer, 8);
		DEBUG("GAP service handle          : %04X\n", ble_ledger_data.gap_service_handle);
		DEBUG("GAP device name char handle : %04X\n", ble_ledger_data.gap_device_name_characteristic_handle);
		DEBUG("GAP appearance char handle  : %04X\n", ble_ledger_data.gap_appearance_characteristic_handle);
	}
	else if (ble_ledger_data.init_step == BLE_INIT_STEP_CONFIGURE_ADVERTISING) {
		ble_ledger_data.adv_enable = !os_setting_get(OS_SETTING_PLANEMODE, NULL, 0);
		configure_advertising_mngr(opcode);
		if (ble_ledger_data.adv_step != BLE_CONFIG_ADV_STEP_END) {
			return;
		}
	}

	ble_ledger_data.init_step++;

	switch (ble_ledger_data.init_step) {

	case BLE_INIT_STEP_RESET:
		ble_hci_forge_cmd_reset(&ble_ledger_data.cmd_data);
		send_hci_packet(0);
		break;

	case BLE_INIT_STEP_STATIC_ADDRESS:
		ble_aci_hal_forge_cmd_write_config_data(&ble_ledger_data.cmd_data,
		                                        BLE_CONFIG_DATA_RANDOM_ADDRESS_OFFSET,
		                                        BLE_CONFIG_DATA_RANDOM_ADDRESS_LEN,
		                                        ble_ledger_data.random_address);
		send_hci_packet(0);
		break;

	case BLE_INIT_STEP_GATT_INIT:
		ble_aci_gatt_forge_cmd_init(&ble_ledger_data.cmd_data);
		send_hci_packet(0);
		break;

	case BLE_INIT_STEP_GAP_INIT:
		ble_aci_gap_forge_cmd_init(&ble_ledger_data.cmd_data,
		                           BLE_GAP_PERIPHERAL_ROLE,
		                           BLE_GAP_PRIVACY_DISABLED,
		                           sizeof(ble_ledger_data.device_name)-1);
		send_hci_packet(0);
		break;

	case BLE_INIT_STEP_SET_IO_CAPABILITIES:
		ble_aci_gap_forge_cmd_set_io_capability(&ble_ledger_data.cmd_data,
		                                        BLE_GAP_IO_CAP_DISPLAY_YES_NO);
		send_hci_packet(0);
		break;

	case BLE_INIT_STEP_SET_AUTH_REQUIREMENTS: {
		ble_cmd_set_auth_req_data_t data;
		data.bonding_mode                   = BLE_GAP_BONDING_ENABLED;
		data.mitm_mode                      = BLE_GAP_MITM_PROTECTION_REQUIRED;
		data.sc_support                     = 1;
		data.key_press_notification_support = BLE_GAP_KEYPRESS_NOT_SUPPORTED;
		data.min_encryption_key_size        = BLE_GAP_MIN_ENCRYPTION_KEY_SIZE;
		data.max_encryption_key_size        = BLE_GAP_MAX_ENCRYPTION_KEY_SIZE;
		data.use_fixed_pin                  = BLE_GAP_USE_FIXED_PIN_FOR_PAIRING_FORBIDDEN;
		data.fixed_pin                      = 0;
		data.identity_address_type          = BLE_GAP_STATIC_RANDOM_ADDR;
		ble_aci_gap_forge_cmd_set_authentication_requirement(&ble_ledger_data.cmd_data, &data);
		send_hci_packet(0);
		break;
	}

	case BLE_INIT_STEP_PROFILE_INIT:
	case BLE_INIT_STEP_PROFILE_CREATE_DB: {
		ble_profile_info_t *profile_info = NULL;
		profile_info = ble_ledger_data.profile[0];
		if (ble_ledger_data.init_step == BLE_INIT_STEP_PROFILE_INIT) {
			if (profile_info->init) {
				((ble_profile_init_t)PIC(profile_info->init))(&ble_ledger_data.cmd_data, profile_info->cookie);
			}
			ble_ledger_data.init_step++;
		}
		if (profile_info->create_db) {
			if (((ble_profile_create_db_t)PIC(profile_info->create_db))(hci_buffer, length,
			                                                            profile_info->cookie) != BLE_PROFILE_STATUS_OK_PROCEDURE_END) {
				ble_ledger_data.init_step--;
				send_hci_packet(0);
			}
			else {
				init_mngr(hci_buffer, length);
			}
		}
		else {
			init_mngr(hci_buffer, length);
		}
		break;
	}

	case BLE_INIT_STEP_SET_TX_POWER_LEVEL:
		ble_aci_hal_forge_cmd_set_tx_power_level(&ble_ledger_data.cmd_data,
		                                         1,  // High power
		                                         0x1A/*7*/); // -14.1 dBm
		ble_ledger_data.connection.requested_tx_power = tx_power_table[0x1A];
		send_hci_packet(0);
		break;

	case BLE_INIT_STEP_CONFIGURE_ADVERTISING:
		ble_ledger_data.cmd_data.hci_cmd_opcode = 0xFFFF;
		ble_ledger_data.adv_step                = BLE_CONFIG_ADV_STEP_IDLE;
		ble_ledger_data.adv_enable              = !os_setting_get(OS_SETTING_PLANEMODE, NULL, 0);
		configure_advertising_mngr(0);
		break;

	case BLE_INIT_STEP_END:
		DEBUG("INIT END\n");
		if (ble_ledger_data.clear_pairing == 0xC1) {
			ble_ledger_data.clear_pairing = 0;
			ble_aci_gap_forge_cmd_clear_security_db(&ble_ledger_data.cmd_data);
			send_hci_packet(0);
		}
		G_io_app.ble_ready = 1;
		ble_ledger_data.state = BLE_STATE_INITIALIZED;
		if (ble_ledger_data.cdc_event_cb) {
			ble_ledger_data.cdc_event_cb(0x00000000, 1);
		}
		break;

	default:
		break;
	}
}

static void configure_advertising_mngr(uint16_t opcode)
{
	uint8_t buffer[31];
	uint8_t index = 0;

	if (  (ble_ledger_data.cmd_data.hci_cmd_opcode != 0xFFFF)
	    &&(opcode != ble_ledger_data.cmd_data.hci_cmd_opcode)
	   ) {
		// Unexpected event => BLE_TODO
		return;
	}

	if (ble_ledger_data.adv_step == BLE_CONFIG_ADV_STEP_IDLE) {
		ble_ledger_data.connection.connection_handle = 0xFFFF;
		ble_ledger_data.advertising_enabled          = 0;
		DEBUG("CONFIGURE ADVERTISING START\n");
	}
	else if (ble_ledger_data.adv_step == (BLE_CONFIG_ADV_STEP_END-1)) {
		ble_ledger_data.advertising_enabled = 1;
	}

	ble_ledger_data.adv_step++;
	if (  (ble_ledger_data.adv_step == (BLE_CONFIG_ADV_STEP_END-1))
	    &&(!ble_ledger_data.adv_enable)
	   ) {
		ble_ledger_data.adv_step++;
	}

	switch (ble_ledger_data.adv_step) {

	case BLE_CONFIG_ADV_STEP_SET_ADV_DATAS:
		// Flags
		buffer[index++] = 2;
		buffer[index++] = BLE_AD_TYPE_FLAGS;
		buffer[index++] = BLE_AD_TYPE_FLAG_BIT_BR_EDR_NOT_SUPPORTED | BLE_AD_TYPE_FLAG_BIT_LE_GENERAL_DISCOVERABLE_MODE;

		// Complete Local Name
		get_device_name();
		buffer[index++] = ble_ledger_data.device_name_length+1;
		buffer[index++] = BLE_AD_TYPE_COMPLETE_LOCAL_NAME;
		memcpy(&buffer[index], ble_ledger_data.device_name, ble_ledger_data.device_name_length);
		index += ble_ledger_data.device_name_length;

		ble_aci_gap_forge_cmd_update_adv_data(&ble_ledger_data.cmd_data, index, buffer);
		send_hci_packet(0);
		break;

	case BLE_CONFIG_ADV_STEP_SET_SCAN_RSP_DATAS: {
		ble_profile_info_t *profile_info = NULL;
		profile_info = ble_ledger_data.profile[0];
		// BLE_TODO
		// Incomplete List of 128-bit Service UUIDs
		buffer[index++] = 16+1;
		buffer[index++] = BLE_AD_AD_TYPE_128_BIT_SERV_UUID;
		memcpy(&buffer[index], (uint8_t*)PIC(profile_info->service_uuid.value), 16);
		index += 16;

		// Slave Connection Interval Range
		buffer[index++] = 5;
		buffer[index++] = BLE_AD_AD_TYPE_SLAVE_CONN_INTERVAL;
		buffer[index++] = BLE_SLAVE_CONN_INTERVAL_MIN;
		buffer[index++] = 0;
		buffer[index++] = BLE_SLAVE_CONN_INTERVAL_MAX;
		buffer[index++] = 0;

		ble_hci_forge_cmd_set_scan_response_data(&ble_ledger_data.cmd_data, index, buffer);
		send_hci_packet(0);
		break;
	}

	case BLE_CONFIG_ADV_STEP_SET_GAP_DEVICE_NAME:
		ble_aci_gatt_forge_cmd_update_char_value(&ble_ledger_data.cmd_data,
		                                         ble_ledger_data.gap_service_handle,
		                                         ble_ledger_data.gap_device_name_characteristic_handle,
		                                         0,
		                                         ble_ledger_data.device_name_length,
		                                         (uint8_t*)ble_ledger_data.device_name);
		send_hci_packet(0);
		break;

	case BLE_CONFIG_ADV_STEP_START:
		advertising_enable(1);
		break;

	default:
		DEBUG("CONFIGURE ADVERTISING END\n");
		if (ble_ledger_data.state == BLE_STATE_CONFIGURE_ADVERTISING) {
			ble_ledger_data.state = BLE_STATE_INITIALIZED;
		}
		break;
	}
}

static void advertising_enable(uint8_t enable)
{
	if (enable) {
		uint8_t buffer[31];

		get_device_name();
		buffer[0] = BLE_AD_TYPE_COMPLETE_LOCAL_NAME;
		memcpy(&buffer[1], ble_ledger_data.device_name, ble_ledger_data.device_name_length);

		ble_cmd_set_discoverable_data_t data;
		data.advertising_type          = BLE_GAP_ADV_IND;
		data.advertising_interval_min  = BLE_ADVERTISING_INTERVAL_MIN;
		data.advertising_interval_max  = BLE_ADVERTISING_INTERVAL_MAX;
		data.own_address_type          = BLE_GAP_RANDOM_ADDR_TYPE;
		data.advertising_filter_policy = BLE_GAP_NO_WHITE_LIST_USE;
		data.local_name_length         = ble_ledger_data.device_name_length+1;
		data.local_name                = buffer;
		data.service_uuid_length       = 0;
		data.service_uuid_list         = NULL;
		data.slave_conn_interval_min   = BLE_SLAVE_CONN_INTERVAL_MIN;
		data.slave_conn_interval_max   = BLE_SLAVE_CONN_INTERVAL_MAX;
		ble_aci_gap_forge_cmd_set_discoverable(&ble_ledger_data.cmd_data, &data);
	}
	else {
		ble_aci_gap_forge_cmd_set_non_discoverable(&ble_ledger_data.cmd_data);
	}
	send_hci_packet(0);
}

static void start_advertising(void)
{
	if (G_io_app.name_changed) {
		G_io_app.name_changed = 0;
		ble_ledger_data.state    = BLE_STATE_CONFIGURE_ADVERTISING;
		ble_ledger_data.adv_step = BLE_CONFIG_ADV_STEP_IDLE;
	}
	else {
		ble_ledger_data.state    = BLE_STATE_CONFIGURE_ADVERTISING;
		ble_ledger_data.adv_step = BLE_CONFIG_ADV_STEP_START-1;
	}
	ble_ledger_data.cmd_data.hci_cmd_opcode = 0xFFFF;
	ble_ledger_data.adv_enable              = !os_setting_get(OS_SETTING_PLANEMODE, NULL, 0);
	configure_advertising_mngr(0);
}
/*
static void ask_user_pairing_numeric_comparison(uint32_t code)
{
	bolos_ux_params_t ux_params;

	SPRINTF(ux_params.u.pairing_request.pairing_info, "%06d", (unsigned int)code);

	ux_params.u.pairing_request.type             = BOLOS_UX_ASYNCHMODAL_PAIRING_REQUEST_NUMCOMP;
	ux_params.u.pairing_request.pairing_info_len = 6;
	ux_params.ux_id                              = BOLOS_UX_ASYNCHMODAL_PAIRING_REQUEST;
	ux_params.len                                = sizeof(ux_params.u.pairing_request);

	G_io_asynch_ux_callback.asynchmodal_end_callback = rsp_user_pairing_numeric_comparison;
	ble_ledger_data.pairing_in_progress = 1;

	os_ux(&ux_params);
}

static void rsp_user_pairing_numeric_comparison(unsigned int status)
{
	end_pairing_ux();
	if (status == BOLOS_UX_OK) {
		ble_aci_gap_forge_cmd_numeric_comparison_value_confirm_yesno(&ble_ledger_data.cmd_data,
		                                                             ble_ledger_data.connection.connection_handle, 1);
	}
	else {
		ble_aci_gap_forge_cmd_numeric_comparison_value_confirm_yesno(&ble_ledger_data.cmd_data,
		                                                             ble_ledger_data.connection.connection_handle, 0);
	}
	send_hci_packet(0);
}*/

static void ask_user_pairing_passkey(void)
{
	bolos_ux_params_t ux_params;

	ble_ledger_data.pairing_code = cx_rng_u32_range_func(0, 1000000, cx_rng_u32);
	SPRINTF(ux_params.u.pairing_request.pairing_info, "%06d", ble_ledger_data.pairing_code);

	ux_params.u.pairing_request.type             = BOLOS_UX_ASYNCHMODAL_PAIRING_REQUEST_PASSKEY;
	ux_params.u.pairing_request.pairing_info_len = 6;
	ux_params.ux_id                              = BOLOS_UX_ASYNCHMODAL_PAIRING_REQUEST;
	ux_params.len                                = sizeof(ux_params.u.pairing_request);

	G_io_asynch_ux_callback.asynchmodal_end_callback = rsp_user_pairing_passkey;
	ble_ledger_data.pairing_in_progress = 1;

	os_ux(&ux_params);
}

static void rsp_user_pairing_passkey(unsigned int status)
{
	end_pairing_ux();

	if (status != BOLOS_UX_OK) { // BLE_TODO
		ble_ledger_data.pairing_code = cx_rng_u32_range_func(0, 1000000, cx_rng_u32);
	}
	ble_aci_gap_forge_cmd_pass_key_resp(&ble_ledger_data.cmd_data,
	                                    ble_ledger_data.connection.connection_handle,
	                                    ble_ledger_data.pairing_code);
	send_hci_packet(0);
}

static void end_pairing_ux(void)
{
	bolos_ux_params_t ux_params;

	if (ble_ledger_data.pairing_in_progress) {
		ux_params.ux_id = BOLOS_UX_ASYNCHMODAL_PAIRING_CANCEL;
		ux_params.len = 0;
		G_io_asynch_ux_callback.asynchmodal_end_callback = NULL;
		ble_ledger_data.pairing_in_progress = 0;
		os_ux(&ux_params);
	}
}

static void hci_evt_cmd_complete(uint8_t *buffer, uint16_t length)
{
	if (length < 3) {
		return;
	}

	uint16_t opcode = U2LE(buffer, 1);

	if (ble_ledger_data.state == BLE_STATE_INITIALIZING) {
		init_mngr(buffer, length);
	}
	else if (ble_ledger_data.state == BLE_STATE_CONFIGURE_ADVERTISING) {
		configure_advertising_mngr(opcode);
	}
	else if (  (  (ble_ledger_data.state == BLE_STATE_STARTING_BRIDGE)
	            ||(ble_ledger_data.state == BLE_STATE_SWITCH_TO_BRIDGE)
	           )
	         &&(opcode == HCI_RESET_CMD_CODE)
	        ) {
		ble_ledger_data.state = BLE_STATE_BRIDGING;
		if (ble_ledger_data.cdc_event_cb) {
			ble_ledger_data.cdc_event_cb(0x00000000, 0);
		}
	}
	else if (opcode == ACI_GATT_WRITE_RESP_CMD_CODE) {
		uint8_t status = BLE_PROFILE_STATUS_OK;
		ble_profile_info_t *profile_info = NULL;
		profile_info = ble_ledger_data.profile[0];
		if (profile_info->write_rsp_ack) {
			status = ((ble_profile_write_rsp_ack_t)PIC(profile_info->write_rsp_ack))(profile_info->cookie);
			if (status == BLE_PROFILE_STATUS_OK_AND_SEND_PACKET) {
				send_hci_packet(0);
			}
		}
	}
	else if (opcode == ACI_GATT_UPDATE_CHAR_VALUE_CMD_CODE) {
		uint8_t status = BLE_PROFILE_STATUS_OK;
		ble_profile_info_t *profile_info = NULL;
		profile_info = ble_ledger_data.profile[0];
		if (profile_info->update_char_val_ack) {
			status = ((ble_profile_update_char_value_ack_t)PIC(profile_info->update_char_val_ack))(profile_info->cookie);
			if (status == BLE_PROFILE_STATUS_OK_AND_SEND_PACKET) {
				send_hci_packet(0);
			}
		}
	}
	else if (  (opcode == ACI_GAP_SET_NON_DISCOVERABLE_CMD_CODE)
	         ||(opcode == ACI_GAP_SET_DISCOVERABLE_CMD_CODE)
	        ) {
		DEBUG("HCI_LE_SET_ADVERTISE_ENABLE %04X %d %d\n", ble_ledger_data.connection.connection_handle,
		                                                  G_io_app.disabling_advertising,
		                                                  G_io_app.enabling_advertising);
		if (ble_ledger_data.connection.connection_handle != 0xFFFF) {
			if (G_io_app.disabling_advertising) {
				// Connected & ordered to disable ble, force disconnection
				BLE_LEDGER_init(); // BLE_TODO
			}
		}
		else if (G_io_app.disabling_advertising) {
			ble_ledger_data.advertising_enabled = 0;
		}
		else if (G_io_app.enabling_advertising) {
			ble_ledger_data.advertising_enabled = 1;
		}
		else {
			ble_ledger_data.advertising_enabled = 1;
		}
		G_io_app.disabling_advertising = 0;
		G_io_app.enabling_advertising  = 0;
	}
	else if (opcode == ACI_GAP_NUMERIC_COMPARISON_VALUE_CONFIRM_YESNO_CMD_CODE) {
		DEBUG("ACI_GAP_NUMERIC_COMPARISON_VALUE_CONFIRM_YESNO\n");
	}
	else if (opcode == ACI_GAP_CLEAR_SECURITY_DB_CMD_CODE) {
		DEBUG("ACI_GAP_CLEAR_SECURITY_DB\n");
	}
	else if (opcode == ACI_GATT_CONFIRM_INDICATION_CMD_CODE) {
		DEBUG("ACI_GATT_CONFIRM_INDICATION\n");
	}
	else if (opcode == HCI_READ_RSSI_CMD_CODE) {
		//DEBUG("HCI_READ_RSSI -%ddBm\n", 0x0100-buffer[6]);
		ble_ledger_data.connection.rssi_level = buffer[6];
	}
	else if (opcode == HCI_LE_READ_ADVERTISING_CHANNEL_TX_POWER_CMD_CODE) {
		//DEBUG("HCI_LE_READ_ADVERTISING_CHANNEL_TX_POWER_CMD_CODE -%ddBm\n", 0x0100-buffer[4]);
		ble_ledger_data.adv_tx_power = buffer[4];
	}
	else if (opcode == HCI_READ_TRANSMIT_POWER_LEVEL_CMD_CODE) {
		/*if (buffer[6] < 0x7F) {
			DEBUG("HCI_READ_TRANSMIT_POWER_LEVEL_CMD_CODE +%ddBm\n", buffer[6]);
		}
		else {
			DEBUG("HCI_READ_TRANSMIT_POWER_LEVEL_CMD_CODE -%ddBm\n", 0x0100-buffer[6]);
		}*/
		if (ble_ledger_data.hci_reading_current_tx_power) {
			ble_ledger_data.connection.current_transmit_power_level = buffer[6];
		}
		else {
			ble_ledger_data.connection.max_transmit_power_level = buffer[6];
		}
	}
	else {
		DEBUG("HCI EVT CMD COMPLETE 0x%04X\n", opcode);
	}
}

static void hci_evt_le_meta_evt(uint8_t *buffer, uint16_t length)
{
	if (!length) {
		return;
	}

	ble_profile_info_t *profile_info = NULL;

	switch (buffer[0]) {

	case HCI_LE_CONNECTION_COMPLETE_SUBEVT_CODE:
		ble_ledger_data.connection.connection_handle     = U2LE(buffer, 2);
		ble_ledger_data.connection.role_slave            = buffer[4];
		ble_ledger_data.connection.peer_address_random   = buffer[5];
		memcpy(ble_ledger_data.connection.peer_address,   &buffer[6], 6);
		ble_ledger_data.connection.conn_interval         = U2LE(buffer, 12);
		ble_ledger_data.connection.conn_latency          = U2LE(buffer, 14);
		ble_ledger_data.connection.supervision_timeout   = U2LE(buffer, 16);
		ble_ledger_data.connection.master_clock_accuracy = buffer[18];
		ble_ledger_data.connection.encrypted             = 0;
		DEBUG("LE CONNECTION COMPLETE %04X - %04X- %04X- %04X\n", ble_ledger_data.connection.connection_handle,
		                                                          ble_ledger_data.connection.conn_interval,
		                                                          ble_ledger_data.connection.conn_latency,
		                                                          ble_ledger_data.connection.supervision_timeout);
		ble_ledger_data.advertising_enabled = 0;

		ble_ledger_data.state = BLE_STATE_CONNECTED;

		profile_info = ble_ledger_data.profile[0];
		if (profile_info->connection_evt) {
			((ble_profile_connection_evt_t)PIC(profile_info->connection_evt))(&ble_ledger_data.connection,
			                                                                  profile_info->cookie);
		}
		if (ble_ledger_data.cdc_event_cb) {
			ble_ledger_data.cdc_event_cb(0x00000001, 0);
		}
		break;

	case HCI_LE_CONNECTION_UPDATE_COMPLETE_SUBEVT_CODE:
		ble_ledger_data.connection.connection_handle     = U2LE(buffer, 2);
		ble_ledger_data.connection.conn_interval         = U2LE(buffer, 4);
		ble_ledger_data.connection.conn_latency          = U2LE(buffer, 6);
		ble_ledger_data.connection.supervision_timeout   = U2LE(buffer, 8);
		DEBUG("LE CONNECTION UPDATE %04X - %04X- %04X- %04X\n", ble_ledger_data.connection.connection_handle,
		                                                        ble_ledger_data.connection.conn_interval,
		                                                        ble_ledger_data.connection.conn_latency,
		                                                        ble_ledger_data.connection.supervision_timeout);

		profile_info = ble_ledger_data.profile[0];
		if (profile_info->connection_update_evt) {
			((ble_profile_connection_update_evt_t)PIC(profile_info->connection_update_evt))(&ble_ledger_data.connection,
			                                                                                profile_info->cookie);
		}
		if (ble_ledger_data.cdc_event_cb) {
			ble_ledger_data.cdc_event_cb(0x00000002, 0);
		}
		break;

	case HCI_LE_DATA_LENGTH_CHANGE_SUBEVT_CODE:
		if (U2LE(buffer, 1) == ble_ledger_data.connection.connection_handle) {
			ble_ledger_data.connection.max_tx_octets = U2LE(buffer, 3);
			ble_ledger_data.connection.max_tx_time   = U2LE(buffer, 5);
			ble_ledger_data.connection.max_rx_octets = U2LE(buffer, 7);
			ble_ledger_data.connection.max_rx_time   = U2LE(buffer, 9);
			DEBUG("LE DATA LENGTH CHANGE %04X - %04X- %04X- %04X\n", ble_ledger_data.connection.max_tx_octets,
			                                                         ble_ledger_data.connection.max_tx_time,
			                                                         ble_ledger_data.connection.max_rx_octets,
			                                                         ble_ledger_data.connection.max_rx_time);
		}
		break;

	case HCI_LE_PHY_UPDATE_COMPLETE_SUBEVT_CODE:
		if (U2LE(buffer, 2) == ble_ledger_data.connection.connection_handle) {
			ble_ledger_data.connection.tx_phy = buffer[4];
			ble_ledger_data.connection.rx_phy = buffer[5];
			DEBUG("LE PHY UPDATE %02X - %02X\n", ble_ledger_data.connection.tx_phy,
			                                     ble_ledger_data.connection.rx_phy);
		}
		break;

	default:
		DEBUG("HCI LE META 0x%02X\n", buffer[0]);
		break;
	}
}

static void hci_evt_vendor(uint8_t *buffer, uint16_t length)
{
	if (length < 4) {
		return;
	}

	uint16_t opcode = U2LE(buffer, 0);

	if (U2LE(buffer, 2) != ble_ledger_data.connection.connection_handle) {
		return;
	}

	switch (opcode) {

	case ACI_GAP_PAIRING_COMPLETE_VSEVT_CODE:
		DEBUG("PAIRING");
		end_pairing_ux();
		switch (buffer[4]) {

		case SMP_PAIRING_STATUS_SUCCESS:
			DEBUG(" SUCCESS\n");
			if (ble_ledger_data.cdc_event_cb) {
				ble_ledger_data.cdc_event_cb(0x00000020, 0);
			}
			break;

		case SMP_PAIRING_STATUS_SMP_TIMEOUT:
			DEBUG(" TIMEOUT\n");
			if (ble_ledger_data.cdc_event_cb) {
				ble_ledger_data.cdc_event_cb(0x00000021, 0);
			}
			break;

		case SMP_PAIRING_STATUS_PAIRING_FAILED:
			DEBUG(" FAILED : %02X\n", buffer[5]);
			if (ble_ledger_data.cdc_event_cb) {
				ble_ledger_data.cdc_event_cb(0x00000022, buffer[5]);
			}
			break;

		default:
			break;
		}
		break;

	case ACI_GAP_PASS_KEY_REQ_VSEVT_CODE:
		DEBUG("PASSKEY REQ\n");
		ask_user_pairing_passkey();
		break;

	case ACI_GAP_NUMERIC_COMPARISON_VALUE_VSEVT_CODE:
		DEBUG("NUMERIC COMP : %d\n", U4LE(buffer, 4));
		if (ble_ledger_data.cdc_event_cb) {
			ble_ledger_data.cdc_event_cb(0x00000030, U4LE(buffer, 4));
		}
		//ask_user_pairing_numeric_comparison(U4LE(buffer, 4));
		break;

	case ACI_L2CAP_CONNECTION_UPDATE_RESP_VSEVT_CODE:
		DEBUG("CONNECTION UPATE RESP %d\n", buffer[4]);
		break;

	case ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE:
	case ACI_GATT_WRITE_PERMIT_REQ_VSEVT_CODE: {
		uint16_t att_handle = U2LE(buffer, 4);
		ble_profile_info_t *profile_info = NULL;
		profile_info = ble_ledger_data.profile[0];
		if (  (profile_info->handle_in_range)
		    &&(((ble_profile_handle_in_range_t)PIC(profile_info->handle_in_range))(att_handle, profile_info->cookie))
		   ) {
			uint8_t status = BLE_PROFILE_STATUS_OK;
			if (opcode == ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE) {
				if (profile_info->att_modified) {
					status = ((ble_profile_att_modified_t)PIC(profile_info->att_modified))(buffer, length,
					                                                                       profile_info->cookie);
				}
			}
			else if (opcode == ACI_GATT_WRITE_PERMIT_REQ_VSEVT_CODE) {
				if (profile_info->write_permit_req) {
					status = ((ble_profile_write_permit_req_t)PIC(profile_info->write_permit_req))(buffer, length,
					                                                                               profile_info->cookie);
				}
			}
			if (status == BLE_PROFILE_STATUS_OK_AND_SEND_PACKET) {
				send_hci_packet(0);
			}
		}
		else if (opcode == ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE) {
			DEBUG("ATT MODIFIED %04X %d bytes at offset %d\n", att_handle, U2LE(buffer, 8), U2LE(buffer, 6));
		}
		break;
	}

	case ACI_ATT_EXCHANGE_MTU_RESP_VSEVT_CODE:
		DEBUG("MTU : %d\n", U2LE(buffer, 4));

		ble_profile_info_t *profile_info = NULL;
		profile_info = ble_ledger_data.profile[0];
		if (profile_info->mtu_changed) {
			uint8_t status = BLE_PROFILE_STATUS_OK;
			status = ((ble_profile_mtu_changed_t)PIC(profile_info->mtu_changed))(U2LE(buffer, 4), profile_info->cookie);
			if (status == BLE_PROFILE_STATUS_OK_AND_SEND_PACKET) {
				send_hci_packet(0);
			}
		}
		break;

	case ACI_GATT_INDICATION_VSEVT_CODE:
		DEBUG("INDICATION EVT\n");
		ble_aci_gatt_forge_cmd_confirm_indication(&ble_ledger_data.cmd_data,
		                                          ble_ledger_data.connection.connection_handle);
		send_hci_packet(0);
		break;

	case ACI_GATT_PROC_COMPLETE_VSEVT_CODE:
		DEBUG("PROCEDURE COMPLETE\n");
		break;

	case ACI_GATT_PROC_TIMEOUT_VSEVT_CODE:
		DEBUG("PROCEDURE TIMEOUT\n");
		BLE_LEDGER_init(); // BLE_TODO
		break;

	default:
		DEBUG("HCI VENDOR 0x%04X\n", opcode);
		break;
	}
}

static uint32_t send_hci_packet(uint32_t timeout_ms)
{
	uint8_t hdr[3];

	UNUSED(timeout_ms);

	hdr[0] = SEPROXYHAL_TAG_BLE_SEND;
	U2BE_ENCODE(hdr, 1, ble_ledger_data.cmd_data.hci_cmd_buffer_length);
	io_seph_send(hdr, 3);
	io_seph_send(ble_ledger_data.cmd_data.hci_cmd_buffer,
	             ble_ledger_data.cmd_data.hci_cmd_buffer_length);

	return SWO_SUCCESS;
}

/* Exported functions --------------------------------------------------------*/
void BLE_LEDGER_init(void)
{
	if (ble_ledger_data.clear_pairing == 0xC1) {
		memset(&ble_ledger_data, 0, sizeof(ble_ledger_data));
		ble_ledger_data.clear_pairing = 0xC1;
	}
	else {
		memset(&ble_ledger_data, 0, sizeof(ble_ledger_data));
	}
}

void BLE_LEDGER_start(uint16_t profile_mask)
{
	LEDGER_BLE_get_mac_address(ble_ledger_data.random_address);
	ble_ledger_data.cmd_data.hci_cmd_opcode = 0xFFFF;
	ble_ledger_data.state                   = BLE_STATE_INITIALIZING;
	ble_ledger_data.init_step               = BLE_INIT_STEP_IDLE;

	if (ble_ledger_data.profiles != profile_mask) {
		if (profile_mask & BLE_LEDGER_PROFILE_APDU) {
			ble_ledger_data.profile[ble_ledger_data.nb_of_profile++] = (ble_profile_info_t*)PIC(&BLE_LEDGER_PROFILE_apdu_info);
		}
#ifdef HAVE_IO_U2F
		if (profile_mask & BLE_LEDGER_PROFILE_U2F) {
			//ble_ledger_data.profile[ble_ledger_data.nb_of_profile++] = (ble_profile_info_t*)PIC(&BLE_LEDGER_PROFILE_u2f_info);
		}
#endif // HAVE_IO_U2F
		init_mngr(NULL, 0);
	}
	ble_ledger_data.profiles = profile_mask;
}

void BLE_LEDGER_swith_to_bridge(void)
{
	ble_ledger_data.state = BLE_STATE_SWITCH_TO_BRIDGE;
	if (ble_ledger_data.connection.connection_handle != 0xFFFF) {
		ble_hci_forge_cmd_disconnect(&ble_ledger_data.cmd_data,
		                             ble_ledger_data.connection.connection_handle,
		                             HCI_DISCONNECTION_REASON_REM_USER_TERM_CONN);
		send_hci_packet(0);
	}
	else {
		ble_hci_forge_cmd_reset(&ble_ledger_data.cmd_data);
		send_hci_packet(0);
	}
}

uint8_t BLE_LEDGER_enable_advertising(uint8_t enable)
{
	uint8_t status = 0;

	if (  (G_io_app.ble_ready)
	    &&(ble_ledger_data.connection.connection_handle == 0xFFFF)
	   ) {
		if (enable) {
			G_io_app.enabling_advertising = 1;
			G_io_app.disabling_advertising = 0;
		}
		else {
			G_io_app.enabling_advertising = 0;
			G_io_app.disabling_advertising = 1;
		}
		advertising_enable(enable);
	}
	else {
		status = 1;
	}

	return status;
}

void BLE_LEDGER_reset_pairings(void)
{
	if (G_io_app.ble_ready) {
		if (ble_ledger_data.connection.connection_handle != 0xFFFF) {
			// Connected => force disconnection before clearing
			ble_ledger_data.clear_pairing = 0xC1;
			BLE_LEDGER_init();
		}
		else {
			ble_aci_gap_forge_cmd_clear_security_db(&ble_ledger_data.cmd_data);
			send_hci_packet(0);
		}
	}
}

uint32_t BLE_LEDGER_receive(uint8_t* buffer, uint16_t buffer_length)
{
	uint32_t status = SWO_SUCCESS;

	if (ble_ledger_data.state == BLE_STATE_BRIDGING) {
		if (   (ble_ledger_data.cdc_controller_packet_cb)
		    && (buffer_length >= 3 )
		   ) {
			ble_ledger_data.cdc_controller_packet_cb(&buffer[3], buffer_length-3);
		}
		return status;
	}

	if (buffer_length < 4) {
		return INVALID_PARAMETER;
	}

	if (buffer[3] == HCI_EVENT_PKT_TYPE) {
		switch (buffer[4]) {

		case HCI_DISCONNECTION_COMPLETE_EVT_CODE:
			if (buffer_length < 9) {
				status = INVALID_PARAMETER;
			}
			else {
				DEBUG("HCI DISCONNECTION COMPLETE code %02X\n", buffer[9]);
				ble_ledger_data.connection.connection_handle = 0xFFFF;
				ble_ledger_data.advertising_enabled          = 0;
				ble_ledger_data.connection.encrypted         = 0;

				if (ble_ledger_data.cdc_event_cb) {
					ble_ledger_data.cdc_event_cb(0x00000003, 0);
				}

				if (ble_ledger_data.state == BLE_STATE_SWITCH_TO_BRIDGE) {
					BLE_LEDGER_swith_to_bridge();
				}
				else {
					ble_ledger_data.state = BLE_STATE_INITIALIZED;
					start_advertising();
				}
			}
			break;

		case HCI_ENCRYPTION_CHANGE_EVT_CODE:
			if (buffer_length < 9) {
				status = INVALID_PARAMETER;
			}
			else if (U2LE(buffer, 7) == ble_ledger_data.connection.connection_handle) {
				if (buffer[9]) {
					DEBUG("Link encrypted\n");
					ble_ledger_data.connection.encrypted = 1;
				}
				else {
					DEBUG("Link not encrypted\n");
					ble_ledger_data.connection.encrypted = 0;
				}
				if (ble_ledger_data.cdc_event_cb) {
					ble_ledger_data.cdc_event_cb(0x00000010, ble_ledger_data.connection.encrypted);
				}
				ble_profile_info_t *profile_info = NULL;
				profile_info = ble_ledger_data.profile[0];
				if (profile_info->encryption_changed) {
					((ble_profile_encryption_changed_t)PIC(profile_info->encryption_changed))(ble_ledger_data.connection.encrypted,
					                                                                          profile_info->cookie);
				}
			}
			else {
				DEBUG("HCI ENCRYPTION CHANGE EVT %d on connection handle \n", buffer[9],
				                                                              U2LE(buffer, 7));
			}
			break;

		case HCI_COMMAND_COMPLETE_EVT_CODE:
			if (buffer_length < 7) {
				status = INVALID_PARAMETER;
			}
			else {
				hci_evt_cmd_complete(&buffer[6], buffer[5]);
			}
			break;

		case HCI_COMMAND_STATUS_EVT_CODE:
			DEBUG("HCI COMMAND_STATUS\n");
			break;

		case HCI_ENCRYPTION_KEY_REFRESH_COMPLETE_EVT_CODE:
			DEBUG("HCI KEY_REFRESH_COMPLETE\n");
			break;

		case HCI_LE_META_EVT_CODE:
			if (buffer_length < 7) {
				status = INVALID_PARAMETER;
			}
			else {
				hci_evt_le_meta_evt(&buffer[6], buffer[5]);
			}
			break;

		case HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE:
			if (buffer_length < 7) {
				status = INVALID_PARAMETER;
			}
			else {
				hci_evt_vendor(&buffer[6], buffer[5]);
			}
			break;

		default:
			break;
		}
	}

	return status;
}

uint32_t BLE_LEDGER_send(uint8_t* packet, uint16_t packet_length, uint32_t timeout_ms)
{
	uint32_t status     = SWO_SUCCESS;
	uint8_t  ble_status = BLE_PROFILE_STATUS_OK;
	ble_profile_info_t *profile_info = NULL;

	profile_info = ble_ledger_data.profile[0];

	if (profile_info->send_packet) {
		ble_status = ((ble_profile_send_packet_t)PIC(profile_info->send_packet))(packet, packet_length,
		                                                                         profile_info->cookie);
	}

	if (ble_status == BLE_PROFILE_STATUS_OK_AND_SEND_PACKET) {
		status = send_hci_packet(timeout_ms);
	}

	return status;
}


void BLE_LEDGER_start_bridge(cdc_controller_packet_cb_t cdc_controller_packet_cb,
                             cdc_event_cb_t             cdc_event_cb)
{
	ble_ledger_data.state                    = BLE_STATE_STARTING_BRIDGE;
	ble_ledger_data.cdc_controller_packet_cb = cdc_controller_packet_cb;
	ble_ledger_data.cdc_event_cb             = cdc_event_cb;
	ble_hci_forge_cmd_reset(&ble_ledger_data.cmd_data);
	send_hci_packet(0);
}

void BLE_LEDGER_send_to_controller(uint8_t* packet, uint16_t packet_length)
{
	uint8_t hdr[3];

	hdr[0] = 0x39;
	U2BE_ENCODE(hdr, 1, packet_length);
	io_seph_send(hdr, 3);
	io_seph_send(packet, packet_length);
}

uint8_t BLE_LEDGER_set_tx_power(uint8_t high_power, int8_t value)
{
	uint8_t status = 0;

	uint8_t index = 0;
	for (index = 0; index < sizeof(tx_power_table); index++) {
		if (tx_power_table[index] == value) {
			break;
		}
	}

	if (index < sizeof(tx_power_table)) {
		if (high_power) {
			ble_aci_hal_forge_cmd_set_tx_power_level(&ble_ledger_data.cmd_data,
			                                         1,
			                                         index);
		}
		else {
			ble_aci_hal_forge_cmd_set_tx_power_level(&ble_ledger_data.cmd_data,
			                                         0,
			                                         index);
		}
		ble_ledger_data.connection.requested_tx_power = value;
		send_hci_packet(0);
	}
	else {
		status = 1;
	}

	return status;
}

uint8_t BLE_LEDGER_requested_tx_power(void)
{
	return ble_ledger_data.connection.requested_tx_power;
}

uint8_t BLE_LEDGER_is_advertising_enabled(void)
{
	return ble_ledger_data.advertising_enabled;
}

uint8_t BLE_LEDGER_disconnect(void)
{
	uint8_t status = 0;

	if (ble_ledger_data.connection.connection_handle != 0xFFFF) {
		ble_hci_forge_cmd_disconnect(&ble_ledger_data.cmd_data,
		                             ble_ledger_data.connection.connection_handle,
		                             0x13);
		send_hci_packet(0);
	}
	else {
		status = 1;
	}

	return status;
}

void BLE_LEDGER_confirm_numeric_comparison(uint8_t confirm)
{
	ble_aci_gap_forge_cmd_numeric_comparison_value_confirm_yesno(&ble_ledger_data.cmd_data,
	                                                             ble_ledger_data.connection.connection_handle, confirm);
	send_hci_packet(0);
}

void BLE_LEDGER_clear_pairings(void)
{
	ble_aci_gap_forge_cmd_clear_security_db(&ble_ledger_data.cmd_data);
	send_hci_packet(0);
}

void BLE_LEDGER_get_connection_info(ble_connection_t *connection_info)
{
	memcpy(connection_info, &ble_ledger_data.connection, sizeof(ble_connection_t));
}

void BLE_LEDGER_trig_read_rssi(void)
{
	if (ble_ledger_data.connection.connection_handle != 0xFFFF) {
		ble_hci_forge_cmd_read_rssi(&ble_ledger_data.cmd_data,
		                            ble_ledger_data.connection.connection_handle);
		send_hci_packet(0);
	}
}

void BLE_LEDGER_trig_read_transmit_power_level(uint8_t current)
{
	if (ble_ledger_data.connection.connection_handle != 0xFFFF) {
		ble_ledger_data.hci_reading_current_tx_power = current;
		ble_hci_forge_cmd_read_transmit_power_level(&ble_ledger_data.cmd_data,
		                                            ble_ledger_data.connection.connection_handle,
		                                            current);
		send_hci_packet(0);
	}
}

uint8_t BLE_LEDGER_update_connection_interval(uint16_t conn_interval_min,
                                              uint16_t conn_interval_max)
{
	uint8_t status = 0;

	if (ble_ledger_data.connection.connection_handle != 0xFFFF) {
		ble_aci_l2cap_forge_cmd_connection_parameter_update(&ble_ledger_data.cmd_data,
		                                                    ble_ledger_data.connection.connection_handle,
		                                                    (conn_interval_min*100)/125, (conn_interval_max*100)/125,
		                                                    ble_ledger_data.connection.conn_latency,
		                                                    ble_ledger_data.connection.supervision_timeout);
		send_hci_packet(0);
	}
	else {
		status = 1;
	}

	return status;
}
