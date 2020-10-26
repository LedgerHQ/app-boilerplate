#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#include <stdint.h>

#include "ux.h"

#include "io.h"

/**
 * Instruction class of the application.
 */
#define CLA 0xE0

/**
 * Maximum length of application name (APPNAME variable in Makefile).
 */
#define MAX_APPNAME_LEN 64

/**
 * Global buffer for interactions between SE and MCU.
 */
extern uint8_t G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

/**
 * Global variable with the lenght of APDU response to send back.
 */
extern uint32_t output_len;

/**
 * Global structure to perform asynchronous UX aside IO operations.
 */
extern ux_state_t G_ux;

/**
 * Global structure with the parameters to exchange with the BOLOS UX application.
 */
extern bolos_ux_params_t G_ux_params;

/**
 * Global enumeration with the state of IO (READY, RECEIVING, WAITING).
 */
extern io_state_e io_state;

#endif  // _GLOBALS_H_
