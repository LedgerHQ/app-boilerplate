#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#include <stdint.h>

#include "ux.h"

#include "io.h"

#define CLA 0xE0

extern uint8_t G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

extern uint32_t output_len;

extern ux_state_t G_ux;

extern bolos_ux_params_t G_ux_params;

extern io_state_e io_state;

#endif  // _GLOBALS_H_
