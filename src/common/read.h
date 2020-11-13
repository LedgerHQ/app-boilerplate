#pragma once

#include <stdint.h>  // uint*_t
#include <stddef.h>  // size_t

/**
 * Function to read 2 bytes as Big Endian.
 *
 * @brief read 2 bytes as Big Endian from `ptr`.
 *
 * @param[in] ptr pointer to array of bytes.
 * @param[in] offset index to read in the array.
 *
 * @return 16-bit unsigned integer value.
 *
 */
uint16_t read_u16_be(const uint8_t *ptr, size_t offset);

/**
 * Function to read 4 bytes as Big Endian.
 *
 * @brief read 4 bytes as Big Endian from `ptr`.
 *
 * @param[in] ptr pointer to array of bytes.
 * @param[in] offset index to read in the array.
 *
 * @return 32-bit unsigned integer value.
 *
 */
uint32_t read_u32_be(const uint8_t *ptr, size_t offset);

/**
 * Function to read 8 bytes as Big Endian.
 *
 * @brief read 8 bytes as Big Endian from `ptr`.
 *
 * @param[in] ptr pointer to array of bytes.
 * @param[in] offset index to read in the array.
 *
 * @return 64-bit unsigned integer value.
 *
 */
uint64_t read_u64_be(const uint8_t *ptr, size_t offset);

/**
 * Function to read 2 bytes as Little Endian.
 *
 * @brief read 2 bytes as Little Endian from `ptr`.
 *
 * @param[in] ptr pointer to array of bytes.
 * @param[in] offset index to read in the array.
 *
 * @return 16-bit unsigned integer value.
 *
 */
uint16_t read_u16_le(const uint8_t *ptr, size_t offset);

/**
 * Function to read 4 bytes as Little Endian.
 *
 * @brief read 4 bytes as Little Endian from `ptr`.
 *
 * @param[in] ptr pointer to array of bytes.
 * @param[in] offset index to read in the array.
 *
 * @return 32-bit unsigned integer value.
 *
 */
uint32_t read_u32_le(const uint8_t *ptr, size_t offset);

/**
 * Function to read 8 bytes as Little Endian.
 *
 * @brief read 8 bytes as Little Endian from `ptr`.
 *
 * @param[in] ptr pointer to array of bytes.
 * @param[in] offset index to read in the array.
 *
 * @return 64-bit unsigned integer value.
 *
 */
uint64_t read_u64_le(const uint8_t *ptr, size_t offset);
