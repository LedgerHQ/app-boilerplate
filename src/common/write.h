#pragma once

#include <stdint.h>  // uint*_t
#include <stddef.h>  // size_t

/**
 * Function to write 16-bit unsigned integer value as Big Endian.
 *
 * @brief write 2 bytes as Big Endian from `value` to `ptr` at `offset`.
 *
 * @param[out] ptr pointer to array of bytes.
 * @param[in]  offset index to write in the array.
 * @param[in]  value 16-bit unsigned integer value to write.
 *
 */
void write_u16_be(const uint8_t *ptr, size_t offset, uint16_t value);

/**
 * Function to write 32-bit unsigned integer value as Big Endian.
 *
 * @brief write 4 bytes as Big Endian from `value` to `ptr` at `offset`.
 *
 * @param[out] ptr pointer to array of bytes.
 * @param[in]  offset index to write in the array.
 * @param[in]  value 32-bit unsigned integer value to write.
 *
 */
void write_u32_be(uint8_t *ptr, size_t offset, uint32_t value);

/**
 * Function to write 64-bit unsigned integer value as Big Endian.
 *
 * @brief write 8 bytes as Big Endian from `value` to `ptr` at `offset`.
 *
 * @param[out] ptr pointer to array of bytes.
 * @param[in]  offset index to write in the array.
 * @param[in]  value 64-bit unsigned integer value to write.
 *
 */
void write_u64_be(uint8_t *ptr, size_t offset, uint64_t value);

/**
 * Function to write 16-bit unsigned integer value as Little Endian.
 *
 * @brief write 2 bytes as Little Endian from `value` to `ptr` at `offset`.
 *
 * @param[out] ptr pointer to array of bytes.
 * @param[in]  offset index to write in the array.
 * @param[in]  value 16-bit unsigned integer value to write.
 *
 */
void write_u16_le(uint8_t *ptr, size_t offset, uint16_t value);

/**
 * Function to write 32-bit unsigned integer value as Little Endian.
 *
 * @brief write 4 bytes as Little Endian from `value` to `ptr` at `offset`.
 *
 * @param[out] ptr pointer to array of bytes.
 * @param[in]  offset index to write in the array.
 * @param[in]  value 32-bit unsigned integer value to write.
 *
 */
void write_u32_le(uint8_t *ptr, size_t offset, uint32_t value);

/**
 * Function to write 64-bit unsigned integer value as Little Endian.
 *
 * @brief write 8 bytes as Little Endian from `value` to `ptr` at `offset`.
 *
 * @param[out] ptr pointer to array of bytes.
 * @param[in]  offset index to write in the array.
 * @param[in]  value 64-bit unsigned integer value to write.
 *
 */
void write_u64_le(uint8_t *ptr, size_t offset, uint64_t value);
