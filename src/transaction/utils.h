#pragma once

#include <stdint.h>   // uint*_t
#include <stdbool.h>  // bool

bool transaction_utils_check_encoding(uint8_t *memo, uint64_t memo_len);

bool transaction_utils_format_memo(uint8_t *memo, uint64_t memo_len, char *dst, uint64_t dst_len);
