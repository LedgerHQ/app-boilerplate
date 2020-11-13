#pragma once

#include <stddef.h>   // size_t
#include <stdint.h>   // int*_t, uint*_t
#include <stdbool.h>  // bool

/**
 * Function to convert 64 bit signed integer as string.
 *
 * @brief 64 bit signed integer to string.
 *
 * @param[out] dst destination string.
 * @param[in]  dst_len length of destination string.
 * @param[in]  value 64 bit signed integer to convert.
 *
 * @return true if success, false otherwise.
 *
 */
bool format_i64(char *dst, size_t dst_len, const int64_t value);

/**
 * Function to convert 64 bit unsigned integer as string.
 *
 * @brief 64 bit unsigned integer to string.
 *
 * @param[out] dst destination string.
 * @param[in]  dst_len length of destination string.
 * @param[in]  value 64 bit unsigned integer to convert.
 * @param[in]  decimals number of digits after decimal separator.
 *
 * @return true if success, false otherwise.
 *
 */
bool format_fpu64(char *dst, size_t dst_len, const uint64_t value, uint8_t decimals);
