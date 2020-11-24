#pragma once

#include <stdint.h>   // uint*_t
#include <stddef.h>   // size_t
#include <stdbool.h>  // bool

/**
 * Function to get the size of value as varint.
 *
 * @brief number of bytes to represent value as varing.
 *
 * @param[in] value 64-bit unsigned integer to compute varint size.
 *
 * @return number of bytes to write value as varint.
 *
 */
uint8_t varint_size(uint64_t value);

/**
 * Function to read Bitcoin varint.
 *
 * @see https://en.bitcoin.it/wiki/Protocol_documentation#Variable_length_integer
 *
 * @brief read varint in array of bytes.
 *
 * @param[in]  in pointer to array of bytes.
 * @param[in]  in_len length of byte array.
 * @param[out] value pointer to 64-bit unsigned integer to store varint.
 *
 * @return number of bytes read, -1 otherwise.
 *
 */
int varint_read(const uint8_t *in, size_t in_len, uint64_t *value);

/**
 * Function to write Bitcoin varint.
 *
 * @see https://en.bitcoin.it/wiki/Protocol_documentation#Variable_length_integer
 *
 * @brief write value as varint with 1, 3, 5 or 9 bytes to ptr.
 *
 * @param[out] ptr pointer to array of bytes.
 * @param[in]  offset index in the array of bytes.
 * @param[in]  value 64-bit unsigned integer to write as varint.
 *
 * @return number of bytes written , -1 otherwise.
 *
 */
int varint_write(uint8_t *out, size_t offset, uint64_t value);
