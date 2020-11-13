#pragma once

#include <stddef.h>   // size_t
#include <stdbool.h>  // bool
#include <stdint.h>   // uint*_t

#include "../types.h"
#include "../common/buffer.h"

/**
 * Function handling GET_PUBLIC_KEY command.
 *
 * @return zero or positive integer if success, negative number otherwise.
 *
 */
int handler_get_public_key(buffer_t *cdata, bool display);
