#pragma once

#include "types.h"
#include "../common/buffer.h"

parser_status_e transaction_deserialize(buffer_t *buf, transaction_t *tx);
