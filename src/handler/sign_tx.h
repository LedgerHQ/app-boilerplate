#pragma once

#include <stdint.h>   // uint*_t
#include <stdbool.h>  // bool

#include "../common/buffer.h"

int handler_sign_tx(buffer_t *cdata, uint8_t chunk, bool more);
