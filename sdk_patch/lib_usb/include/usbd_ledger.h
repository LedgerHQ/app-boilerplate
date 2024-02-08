/* @BANNER@ */

#ifndef USBD_LEDGER_H
#define USBD_LEDGER_H

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "usbd_def.h"

/* Exported enumerations -----------------------------------------------------*/
typedef enum {
	USBD_LEDGER_PRODUCT_BLUE       = 0x0000,
	USBD_LEDGER_PRODUCT_NANOS      = 0x1000,
	USBD_LEDGER_PRODUCT_HW2        = 0x3000,
	USBD_LEDGER_PRODUCT_NANOX      = 0x4000,
	USBD_LEDGER_PRODUCT_NANOS_PLUS = 0x5000,
	USBD_LEDGER_PRODUCT_STAX       = 0x6000,
	USBD_LEDGER_PRODUCT_EUROPA     = 0x7000,
} usbd_ledger_product_e;

typedef enum {
	USBD_LEDGER_CLASS_HID         = 0x0001,
	USBD_LEDGER_CLASS_HID_KBD     = 0x0002,
	USBD_LEDGER_CLASS_U2F         = 0x0004,
	USBD_LEDGER_CLASS_CCID        = 0x0008,
	USBD_LEDGER_CLASS_WEBUSB      = 0x0010,
	USBD_LEDGER_CLASS_CDC_CONTROL = 0x0020,
	USBD_LEDGER_CLASS_CDC_DATA    = 0x0040,
	USBD_LEDGER_CLASS_CDC         = USBD_LEDGER_CLASS_CDC_CONTROL|USBD_LEDGER_CLASS_CDC_DATA,
} usbd_ledger_class_mask_e;

/* Exported defines   --------------------------------------------------------*/

/* Exported types, structures, unions ----------------------------------------*/

/* Exported macros------------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/
extern uint8_t USBD_LEDGER_apdu_buffer[IO_APDU_BUFFER_SIZE];

/* Exported functions prototypes--------------------------------------------- */
void USBD_LEDGER_init(void);
void USBD_LEDGER_start(uint16_t pid,
                       uint16_t vid,
                       char*    name,
                       uint16_t class_mask); // mask forged with usbd_ledger_class_mask_e

// Rx
void USBD_LEDGER_rx_evt_reset(void);
void USBD_LEDGER_rx_evt_sof(void);
void USBD_LEDGER_rx_evt_suspend(void);
void USBD_LEDGER_rx_evt_resume(void);
void USBD_LEDGER_rx_evt_setup(uint8_t* buffer);
void USBD_LEDGER_rx_evt_data_in(uint8_t ep_num, uint8_t* buffer);
void USBD_LEDGER_rx_evt_data_out(uint8_t ep_num, uint8_t* buffer, uint16_t length);

// Tx
uint32_t USBD_LEDGER_send(uint8_t class_type, uint8_t* packet, uint16_t packet_length, uint32_t timeout_ms);

#endif // USBD_LEDGER_H

