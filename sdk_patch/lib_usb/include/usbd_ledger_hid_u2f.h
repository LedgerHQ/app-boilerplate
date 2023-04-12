/* @BANNER@ */

#ifndef USBD_LEDGER_HID_U2F_H
#define USBD_LEDGER_HID_U2F_H

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "usbd_def.h"

/* Exported enumerations -----------------------------------------------------*/

/* Exported defines   --------------------------------------------------------*/
#define USBD_LEDGER_HID_U2F_EPIN_ADDR  (0x81)
#define USBD_LEDGER_HID_U2F_EPIN_SIZE  (0x0040)
#define USBD_LEDGER_HID_U2F_EPOUT_ADDR (0x01)
#define USBD_LEDGER_HID_U2F_EPOUT_SIZE (0x0040)

/* Exported types, structures, unions ----------------------------------------*/

/* Exported macros------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/
const usbd_end_point_info_t USBD_LEDGER_HID_U2F_end_point_info = {
	.ep_in_addr      = USBD_LEDGER_HID_U2F_EPIN_ADDR,
	.ep_in_size      = USBD_LEDGER_HID_U2F_EPIN_SIZE,
	.ep_out_addr     = USBD_LEDGER_HID_U2F_EPOUT_ADDR,
	.ep_out_size     = USBD_LEDGER_HID_U2F_EPOUT_SIZE,
	.ep_type         = USBD_EP_TYPE_INTR,
};

const uint8_t USBD_LEDGER_HID_U2F_report_descriptor[34] = {
	0x06, 0xD0, 0xF1,                              // Usage page      : vendor defined
	0x09, 0x01,                                    // Usage ID        : vendor defined
	0xA1, 0x01,                                    // Collection      : application

	// The Input report
	0x09, 0x03,                                    // Usage ID        : vendor defined
	0x15, 0x00,                                    // Logical Minimum : 0
	0x26, 0xFF, 0x00,                              // Logical Maximum : 255
	0x75, 0x08,                                    // Report Size     : 8 bits
	0x95, USBD_LEDGER_HID_U2F_EPIN_SIZE,           // Report Count    : 64 fields
	0x81, 0x08,                                    // Input           : Data, Array, Absolute, Wrap

	// The Output report
	0x09, 0x04,                                    // Usage ID - vendor defined
	0x15, 0x00,                                    // Logical Minimum (0)
	0x26, 0xFF, 0x00,                              // Logical Maximum (255)
	0x75, 0x08,                                    // Report Size (8 bits)
	0x95, USBD_LEDGER_HID_U2F_EPOUT_SIZE,          // Report Count (64 fields)
	0x91, 0x08,                                    // Output (Data, Variable, Absolute)
	0xC0
};

const uint8_t USBD_LEDGER_HID_U2F_descriptors[32] = {
	USB_LEN_IF_DESC,                               // bLength
	USB_DESC_TYPE_INTERFACE,                       // bDescriptorType    : interface
	0x00,                                          // bInterfaceNumber   : 0 (dynamic)
	0x00,                                          // bAlternateSetting  : 0
	0x02,                                          // bNumEndpoints      : 2
	0x03,                                          // bInterfaceClass    : HID
	0x01,                                          // bInterfaceSubClass : boot
	0x01,                                          // bInterfaceProtocol : keyboard
	USBD_IDX_PRODUCT_STR,                          // iInterface         :

	0x09,                                          // bLength:
	0x21,                                          // bDescriptorType : HID
	0x11,                                          // bcdHID
	0x01,                                          // bcdHID          : V1.11
	0x21,                                          // bCountryCode    : US
	0x01,                                          // bNumDescriptors : 1
	0x22,                                          // bDescriptorType : report
	sizeof(USBD_LEDGER_HID_U2F_report_descriptor), // wItemLength
	0x00,

	USB_LEN_EP_DESC,                               // bLength
	USB_DESC_TYPE_ENDPOINT,                        // bDescriptorType
	USBD_LEDGER_HID_U2F_EPIN_ADDR,                 // bEndpointAddress
	USBD_EP_TYPE_INTR,                             // bmAttributes
	USBD_LEDGER_HID_U2F_EPIN_SIZE,                 // wMaxPacketSize
	0x00,                                          // wMaxPacketSize
	0x01,                                          // bInterval

	USB_LEN_EP_DESC,                               // bLength
	USB_DESC_TYPE_ENDPOINT,                        // bDescriptorType
	USBD_LEDGER_HID_U2F_EPOUT_ADDR,                // bEndpointAddress
	USBD_EP_TYPE_INTR,                             // bmAttributes
	USBD_LEDGER_HID_U2F_EPOUT_SIZE,                // wMaxPacketSize
	0x00,                                          // wMaxPacketSize
	0x01,                                          // bInterval
};

/* Exported functions prototypes--------------------------------------------- */
// Init
// DeInit
// DataIn
// DataOut

#endif // USBD_LEDGER_HID_U2F_H

