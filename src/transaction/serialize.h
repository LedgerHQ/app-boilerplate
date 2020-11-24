#pragma once

#include <stdint.h>   // uint*_t
#include <stdbool.h>  // bool
#include <stddef.h>   // size_t

#include "types.h"

int transaction_serialize(transaction_t *tx, uint8_t *out, size_t out_len);
