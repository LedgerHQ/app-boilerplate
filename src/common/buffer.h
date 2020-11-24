#pragma once

#include <stdint.h>   // uint*_t
#include <stddef.h>   // size_t
#include <stdbool.h>  // bool

/**
 * Enumeration for endianness.
 *
 * @brief either Big Endian (BE) or Little Endian (LE).
 *
 */
typedef enum { BE, LE } endianness_t;

/**
 * Struct representing a buffer.
 *
 * @brief buffer representation with size and offset.
 *
 */
typedef struct {
    const uint8_t *ptr;
    size_t size;
    size_t offset;
} buffer_t;

/**
 * Function to know if buffer is readable.
 *
 * @brief whether buffer can read `n` bytes or not.
 *
 * @param[in] buffer pointer to input buffer.
 * @param[in] n length to read in buffer.
 *
 * @return true if success, false otherwise.
 *
 */
bool buffer_can_read(const buffer_t *buffer, size_t n);

/**
 * Function to seek the buffer to specific offset.
 *
 * @brief seek buffer to `offset`.
 *
 * @param[in,out] buffer pointer to input buffer.
 * @param[in]     offset position to seek.
 *
 * @return true if success, false otherwise.
 *
 */
bool buffer_seek_set(buffer_t *buffer, size_t offset);

/**
 * Function to seek the buffer relatively to current offset.
 *
 * @brief seek buffer to `buffer->offset` + `offset`.
 *
 * @param[in,out] buffer pointer to input buffer.
 * @param[in]     offset position to seek relatively from `buffer->offset`.
 *
 * @return true if success, false otherwise.
 *
 */
bool buffer_seek_cur(buffer_t *buffer, size_t offset);

/**
 * Function to seek the buffer relatively to the end.
 *
 * @brief seek buffer to `buffer->size` - `offset`.
 *
 * @param[in,out] buffer pointer to input buffer.
 * @param[in]     offset position to seek relatively to `buffer->size`.
 *
 * @return true if success, false otherwise.
 *
 */
bool buffer_seek_end(buffer_t *buffer, size_t offset);

/**
 * Function to read 1 byte from buffer.
 *
 * @brief read uint8_t from buffer if possible.
 *
 * @param[in,out]  buffer pointer to input buffer.
 * @param[out]     value 8 bit value read from buffer.
 *
 * @return true if success, false otherwise.
 *
 */
bool buffer_read_u8(buffer_t *buffer, uint8_t *value);

/**
 * Function to read 2 bytes from buffer.
 *
 * @brief read uint16_t from buffer if possible.
 *
 * @param[in,out]  buffer pointer to input buffer.
 * @param[out]     value 16 bit value read from buffer.
 * @param[in]      endianness either BE or LE.
 *
 * @return true if success, false otherwise.
 *
 */
bool buffer_read_u16(buffer_t *buffer, uint16_t *value, endianness_t endianness);

/**
 * Function to read 4 bytes from buffer.
 *
 * @brief read uint32_t from buffer if possible.
 *
 * @param[in,out]  buffer pointer to input buffer.
 * @param[out]     value 32 bit value read from buffer.
 * @param[in]      endianness either BE or LE.
 *
 * @return true if success, false otherwise.
 *
 */
bool buffer_read_u32(buffer_t *buffer, uint32_t *value, endianness_t endianness);

/**
 * Function to read 8 bytes from buffer.
 *
 * @brief read uint16_t from buffer if possible.
 *
 * @param[in,out]  buffer pointer to input buffer.
 * @param[out]     value 64 bit value read from buffer.
 * @param[in]      endianness either BE or LE.
 *
 * @return true if success, false otherwise.
 *
 */
bool buffer_read_u64(buffer_t *buffer, uint64_t *value, endianness_t endianness);

/**
 * Function to read Bitcoin-like varint from buffer.
 *
 * @see https://en.bitcoin.it/wiki/Protocol_documentation#Variable_length_integer
 *
 * @brief read 1, 2, 4 or 8 bytes from buffer if possible.
 *
 * @param[in,out]  buffer pointer to input buffer.
 * @param[out]     value 64 bit value read from buffer.
 *
 * @return true if success, false otherwise.
 *
 */
bool buffer_read_varint(buffer_t *buffer, uint64_t *value);

bool buffer_read_bip32_path(buffer_t *buffer, uint32_t *out, size_t out_len);

/**
 * Function to copy `out_len` bytes from buffer.
 *
 * @brief copy `out_len` bytes from buffer in `out`.
 *
 * @param[in]  buffer pointer to input buffer.
 * @param[out] out array to copy data from buffer.
 * @param[in]  out_len length of array.
 *
 * @return true if success, false otherwise.
 *
 */
bool buffer_copy(const buffer_t *buffer, uint8_t *out, size_t out_len);

/**
 * Function to move `out_len` bytes from buffer.
 *
 * @brief copy `out_len` bytes from buffer in `out` and move `buffer->offset` accordingly.
 *
 * @param[in,out]  buffer pointer to input buffer.
 * @param[out]     out array to move data from buffer.
 * @param[in]      out_len length of array.
 *
 * @return true if success, false otherwise.
 *
 */
bool buffer_move(buffer_t *buffer, uint8_t *out, size_t out_len);
