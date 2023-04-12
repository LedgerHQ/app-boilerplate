/* @BANNER@ */

/* Includes ------------------------------------------------------------------*/
#include "os_io_seproxyhal.h"
#include "usbd_ioreq.h"
#include "usbd_ledger.h"
#include "usbd_ledger_hid_kbd.h"

#ifdef HAVE_USB_HIDKBD

#pragma GCC diagnostic ignored "-Wcast-qual"

/* Private enumerations ------------------------------------------------------*/
enum ledger_hid_kbd_state_t
{
	LEDGER_HID_KBD_STATE_IDLE,
	LEDGER_HID_KBD_STATE_BUSY,
};

/* Private defines------------------------------------------------------------*/
#define LEDGER_HID_KBD_EPIN_ADDR  (0x82)
#define LEDGER_HID_KBD_EPIN_SIZE  (0x08)
#define LEDGER_HID_KBD_EPOUT_ADDR (0x02)
#define LEDGER_HID_KBD_EPOUT_SIZE (0x08)

#define HID_DESCRIPTOR_TYPE           (0x21)
#define HID_REPORT_DESCRIPTOR_TYPE    (0x22)

// HID Class-Specific Requests
#define REQ_SET_REPORT                (0x09)
#define REQ_GET_REPORT                (0x01)
#define REQ_SET_IDLE                  (0x0A)
#define REQ_GET_IDLE                  (0x02)
#define REQ_SET_PROTOCOL              (0x0B)
#define REQ_GET_PROTOCOL              (0x03)

/* Private types, structures, unions -----------------------------------------*/
typedef struct
{
	uint8_t protocol;
	uint8_t idle_state;
	uint8_t alt_setting;
	uint8_t state; // ledger_hid_kbd_state_t
} ledger_hid_kbd_handle_t;

/* Private macros-------------------------------------------------------------*/

/* Private functions prototypes ----------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static ledger_hid_kbd_handle_t ledger_hid_kbd_handle;

/* Exported variables --------------------------------------------------------*/
const usbd_end_point_info_t LEDGER_HID_KBD_end_point_info = {
	.ep_in_addr      = LEDGER_HID_KBD_EPIN_ADDR,
	.ep_in_size      = LEDGER_HID_KBD_EPIN_SIZE,
	.ep_out_addr     = LEDGER_HID_KBD_EPOUT_ADDR,
	.ep_out_size     = LEDGER_HID_KBD_EPOUT_SIZE,
	.ep_type         = USBD_EP_TYPE_INTR,
};

const uint8_t LEDGER_HID_KBD_report_descriptor[63] = {
	0x05, 0x01,                                    // Usage page      : Generic Desktop
	0x09, 0x06,                                    // Usage ID        : Keyboard
	0xA1, 0x01,                                    // Collection      : application

	0x05, 0x07,                                    // Usage page      : Keyboard
	0x19, 0xE0,                                    // Usage minimum   : Keyboard LeftControl
	0x29, 0xE7,                                    // Usage maximum   : Keyboard Right GUI
	0x15, 0x00,                                    // Logical minimum : 0
	0x25, 0x01,                                    // Logical maximum : 1
	0x75, 0x01,                                    // Report size     : 1 bit
	0x95, 0x08,                                    // Report count    : 8 fields
	0x81, 0x02,                                    // Input           : Data, Variable, Absolute
	0x95, 0x01,                                    // Report count    : 1 fields
	0x75, 0x08,                                    // Report size     : 8 bit
	0x81, 0x01,                                    // Input           : onstant, Array, Absolute
	0x95, 0x05,                                    // Report count    : 5 fields
	0x75, 0x01,                                    // Report size     : 1 bit

	0x05, 0x08,                                    // Usage page      : LEDs
	0x19, 0x01,                                    // Usage minimum   : Num Lock
	0x29, 0x05,                                    // Usage maximum   : Kana
	0x91, 0x02,                                    // Output          : Data, Variable, Absolute
	0x95, 0x01,                                    // Report count    : 1 fields
	0x75, 0x03,                                    // Report size     : 3 bit
	0x91, 0x01,                                    // Output          : Constant, Array, Absolute
	0x95, 0x06,                                    // Report count    : 6 fields
	0x75, 0x08,                                    // Report size     : 8 bit
	0x15, 0x00,                                    // Logical minimum : 0
	0x25, 0x65,                                    // Logical maximum : 101

	0x05, 0x07,                                    // Usage page      : Keyboard
	0x19, 0x00,                                    // Usage minimum   : Reserved (no event indicated)
	0x29, 0x65,                                    // Usage maximum   : Keyboard application
	0x81, 0x00,                                    // Input           : Data, Array, Absolute
	0xC0                                           // Collection      : end
};

const uint8_t LEDGER_HID_KBD_descriptors[32] = {
	USB_LEN_IF_DESC,                                // bLength
	USB_DESC_TYPE_INTERFACE,                       // bDescriptorType    : interface
	0x00,                                          // bInterfaceNumber   : 0
	0x00,                                          // bAlternateSetting  : 0
	0x02,                                          // bNumEndpoints      : 2
	0x03,                                          // bInterfaceClass    : HID
	0x01,                                          // bInterfaceSubClass : boot
	0x01,                                          // bInterfaceProtocol : keyboard
	USBD_IDX_PRODUCT_STR,                          // iInterface         : no string

	0x09,                                          // bLength:
	0x21,                                          // bDescriptorType : HID
	0x11,                                          // bcdHID
	0x01,                                          // bcdHID          : V1.11
	0x21,                                          // bCountryCode    : US
	0x01,                                          // bNumDescriptors : 1
	0x22,                                          // bDescriptorType : report
	sizeof(LEDGER_HID_KBD_report_descriptor),      // wItemLength
	0x00,

	USB_LEN_EP_DESC,                               // bLength
	USB_DESC_TYPE_ENDPOINT,                        // bDescriptorType
	LEDGER_HID_KBD_EPIN_ADDR,                      // bEndpointAddress
	USBD_EP_TYPE_INTR,                             // bmAttributes
	LEDGER_HID_KBD_EPIN_SIZE,                      // wMaxPacketSize
	0x00,                                          // wMaxPacketSize
	0x01,                                          // bInterval

	USB_LEN_EP_DESC,                               // bLength
	USB_DESC_TYPE_ENDPOINT,                        // bDescriptorType
	LEDGER_HID_KBD_EPOUT_ADDR,                     // bEndpointAddress
	USBD_EP_TYPE_INTR,                             // bmAttributes
	LEDGER_HID_KBD_EPOUT_SIZE,                     // wMaxPacketSize
	0x00,                                          // wMaxPacketSize
	0x01,                                          // bInterval
};

const usbd_class_info_t USBD_LEDGER_HID_KBD_class_info = {
	.type      = USBD_LEDGER_CLASS_HID_KBD,

	.end_point = &LEDGER_HID_KBD_end_point_info,

	.init     = USBD_LEDGER_HID_KBD_init,
	.de_init  = USBD_LEDGER_HID_KBD_de_init,
	.setup    = USBD_LEDGER_HID_KBD_setup,
	.data_in  = USBD_LEDGER_HID_KBD_data_in,
	.data_out = USBD_LEDGER_HID_KBD_data_out,

	.send_packet = USBD_LEDGER_HID_KBD_send_packet,

	.interface_descriptor      = LEDGER_HID_KBD_descriptors,
	.interface_descriptor_size = sizeof(LEDGER_HID_KBD_descriptors),

	.interface_association_descriptor      = NULL,
	.interface_association_descriptor_size = 0,

	.bos_descriptor      = NULL,
	.bos_descriptor_size = 0,

	.cookie = &ledger_hid_kbd_handle,
};

/* Private functions ---------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/
uint8_t USBD_LEDGER_HID_KBD_init(USBD_HandleTypeDef *pdev, void* cookie)
{
	if (!pdev || !cookie) {
		return USBD_FAIL;
	}

	UNUSED(pdev);

	ledger_hid_kbd_handle_t *handle = (ledger_hid_kbd_handle_t*)PIC(cookie);

	memset(handle, 0, sizeof(ledger_hid_kbd_handle_t));
	USBD_LL_PrepareReceive(pdev, LEDGER_HID_KBD_EPOUT_ADDR,
	                       NULL, LEDGER_HID_KBD_EPOUT_SIZE);

	return USBD_OK;
}

uint8_t USBD_LEDGER_HID_KBD_de_init(USBD_HandleTypeDef *pdev, void* cookie)
{
	UNUSED(pdev);
	UNUSED(cookie);

	return USBD_OK;
}

uint8_t USBD_LEDGER_HID_KBD_setup(USBD_HandleTypeDef *pdev, void* cookie,
                                  USBD_SetupReqTypedef *req)
{
	if (!pdev || !cookie || !req) {
		return USBD_FAIL;
	}

	uint8_t                 ret     = USBD_OK;
	ledger_hid_kbd_handle_t *handle = (ledger_hid_kbd_handle_t*)PIC(cookie);

	uint8_t request = req->bmRequest & (USB_REQ_TYPE_MASK|USB_REQ_RECIPIENT_MASK);

	// HID Standard Requests
	if (request == (USB_REQ_TYPE_STANDARD|USB_REQ_RECIPIENT_INTERFACE)) {
		switch (req->bRequest) {

		case USB_REQ_GET_STATUS:
			if (pdev->dev_state == USBD_STATE_CONFIGURED) {
				uint16_t status_info = 0x0000;
				USBD_CtlSendData(pdev, (uint8_t *)(void *)&status_info,
				                 sizeof(status_info));
			}
			else {
				ret = USBD_FAIL;
			}
			break;

		case USB_REQ_GET_DESCRIPTOR:
			if (req->wValue == ((uint16_t)(HID_DESCRIPTOR_TYPE << 8))) {
				USBD_CtlSendData(pdev, (uint8_t *)PIC(&LEDGER_HID_KBD_descriptors[USB_LEN_IF_DESC]),
				                 MIN(LEDGER_HID_KBD_descriptors[USB_LEN_IF_DESC], req->wLength));
			}
			else if (req->wValue == ((uint16_t)(HID_REPORT_DESCRIPTOR_TYPE << 8))) {
				USBD_CtlSendData(pdev, (uint8_t *)PIC(LEDGER_HID_KBD_report_descriptor),
				                 MIN(sizeof(LEDGER_HID_KBD_report_descriptor), req->wLength));
			}
			else {
				ret = USBD_FAIL;
			}
			break;

		case USB_REQ_GET_INTERFACE :
			if (pdev->dev_state == USBD_STATE_CONFIGURED) {
				USBD_CtlSendData(pdev, &handle->alt_setting,
				                 sizeof(handle->alt_setting));
			}
			else {
				ret = USBD_FAIL;
			}
			break;

		case USB_REQ_SET_INTERFACE :
			if (pdev->dev_state == USBD_STATE_CONFIGURED) {
				handle->alt_setting = (uint8_t)(req->wValue);
			}
			else {
				ret = USBD_FAIL;
			}
			break;

		case USB_REQ_CLEAR_FEATURE:
			break;

		default:
			ret = USBD_FAIL;
			break;
		}
	}
	// HID Class-Specific Requests
	else if (request == (USB_REQ_TYPE_CLASS|USB_REQ_RECIPIENT_INTERFACE)) {
		switch (req->bRequest) {

		case REQ_SET_PROTOCOL:
			handle->protocol = (uint8_t)(req->wValue);
			break;

		case REQ_GET_PROTOCOL:
			USBD_CtlSendData(pdev, &handle->protocol,
			                 sizeof(handle->protocol));
			break;

		case REQ_SET_IDLE:
			handle->idle_state = (uint8_t)(req->wValue >> 8);
			break;

		case REQ_GET_IDLE:
			USBD_CtlSendData(pdev, &handle->idle_state,
			                 sizeof(handle->idle_state));
			break;

		default:
			ret = USBD_FAIL;
			break;
		}
	}
	else {
		ret = USBD_FAIL;
	}

	return ret;
}

uint8_t USBD_LEDGER_HID_KBD_ep0_rx_ready(USBD_HandleTypeDef *pdev, void* cookie)
{
	UNUSED(pdev);
	UNUSED(cookie);

	return USBD_OK;
}

uint8_t USBD_LEDGER_HID_KBD_data_in(USBD_HandleTypeDef *pdev, void* cookie, uint8_t ep_num)
{
	if (!pdev || !cookie) {
		return USBD_FAIL;
	}

	UNUSED(pdev);
	UNUSED(ep_num);

	ledger_hid_kbd_handle_t *handle = (ledger_hid_kbd_handle_t*)PIC(cookie);

	handle->state = LEDGER_HID_KBD_STATE_IDLE;

	return USBD_OK;
}

uint8_t USBD_LEDGER_HID_KBD_data_out(USBD_HandleTypeDef *pdev, void* cookie, uint8_t ep_num,
                                     uint8_t* packet, uint16_t packet_length)
{
	if (!pdev) {
		return USBD_FAIL;
	}

	UNUSED(cookie);
	UNUSED(ep_num);
	UNUSED(packet);
	UNUSED(packet_length);

	USBD_LL_PrepareReceive(pdev, LEDGER_HID_KBD_EPOUT_ADDR, NULL,
	                       LEDGER_HID_KBD_EPOUT_SIZE);

	return USBD_OK;
}

uint8_t USBD_LEDGER_HID_KBD_send_packet(USBD_HandleTypeDef *pdev, void* cookie,
                                        uint8_t* packet, uint16_t packet_length,
                                        uint32_t timeout_ms)
{
	if (!pdev || !cookie || !packet) {
		return USBD_FAIL;
	}

	UNUSED(packet_length);

	uint8_t                  ret     = USBD_OK;
	ledger_hid_kbd_handle_t *handle = (ledger_hid_kbd_handle_t*)PIC(cookie);

	if (pdev->dev_state == USBD_STATE_CONFIGURED) {
		if (handle->state == LEDGER_HID_KBD_STATE_IDLE) {
			ret = USBD_LL_Transmit(pdev, LEDGER_HID_KBD_EPIN_ADDR,
			                       packet,
			                       LEDGER_HID_KBD_EPIN_SIZE, timeout_ms);
		}
		else {
			ret = USBD_BUSY;
		}
	}
	else {
		ret = USBD_FAIL;
	}

	return ret;
}

#endif // HAVE_USB_HIDKBD
