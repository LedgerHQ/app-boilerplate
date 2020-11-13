#pragma once

#include <stdint.h>

#include "ux.h"

#include "io.h"
#include "types.h"

/**
 * Instruction class of the Boilerplate application.
 */
#define CLA 0xE0

/**
 * Length of APPNAME variable in Makefile.
 */
#define APPNAME_LEN (sizeof(APPNAME) - 1)

/**
 * Length of (MAJOR_VERSION || MINOR_VERSION || PATCH_VERSION) variables in Makefile.
 */
#define APPVERSION_LEN 3

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
extern uint32_t G_output_len;

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
extern io_state_e G_io_state;
