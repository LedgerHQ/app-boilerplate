#pragma once

#include <stddef.h>   // size_t
#include <stdint.h>   // int*_t, uint*_t
#include <stdbool.h>  // bool

/**
 * Format 64-bit signed integer as string.
 *
 * @param[out] dst
 *   Pointer to output string.
 * @param[in]  dst_len
 *   Length of output string.
 * @param[in]  value
 *   64-bit signed integer to format.
 *
 * @return true if success, false otherwise.
 *
 */
bool format_i64(char *dst, size_t dst_len, const int64_t value);

/**
 * Format 64-bit unsigned integer as string with decimals.
 *
 * @param[out] dst
 *   Pointer to output string.
 * @param[in]  dst_len
 *   Length of output string.
 * @param[in]  value
 *   64-bit unsigned integer to format.
 * @param[in]  decimals
 *   Number of digits after decimal separator.
 *
 * @return true if success, false otherwise.
 *
 */
bool format_fpu64(char *dst, size_t dst_len, const uint64_t value, uint8_t decimals);
