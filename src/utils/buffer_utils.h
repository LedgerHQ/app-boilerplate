#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "buffer.h"

/**
 * Buffer utilities for safe read/write operations.
 *
 * This module provides a consistent interface for buffer operations,
 * following the pattern established in the Bitcoin Ledger app and the
 * Ledger SDK.
 *
 * - For reading: Use SDK's buffer_t with const ptr (from buffer.h)
 * - For writing: Use write_buffer_t with mutable ptr (this module)
 *
 * CBOR-specific helpers wrap the cbor.c functions with buffer safety.
 *
 * Replaces legacy bufView.h which has been removed.
 */

// Define write-specific buffer with mutable ptr
typedef struct {
    uint8_t *ptr;    // Mutable pointer for write operations
    size_t size;     // Size of buffer
    size_t offset;   // Current offset
} write_buffer_t;

/**
 * Initialize a buffer for writing.
 *
 * @param[in] ptr Pointer to buffer memory
 * @param[in] size Size of buffer in bytes
 * @return Initialized write_buffer_t with offset=0
 */
static inline write_buffer_t buffer_init(void *ptr, size_t size) {
    return (write_buffer_t){.ptr = (uint8_t *)ptr, .size = size, .offset = 0};
}

/**
 * Check if buffer can write n bytes.
 *
 * @param[in] buffer Pointer to buffer struct
 * @param[in] n Number of bytes to check
 * @return true if n bytes available, false otherwise
 */
static inline bool buffer_can_write(const write_buffer_t *buffer, size_t n) {
    return buffer->size - buffer->offset >= n;
}

/**
 * Get number of bytes written so far.
 *
 * @param[in] buffer Pointer to buffer struct
 * @return Number of bytes written (offset)
 */
static inline size_t buffer_written_size(const write_buffer_t *buffer) {
    return buffer->offset;
}

/**
 * Get remaining space in buffer.
 *
 * @param[in] buffer Pointer to buffer struct
 * @return Number of bytes remaining
 */
static inline size_t buffer_remaining_size(const write_buffer_t *buffer) {
    return buffer->size - buffer->offset;
}

/**
 * Write 1 byte to buffer.
 *
 * @param[in,out] buffer Pointer to buffer struct
 * @param[in] value Byte to write
 * @return true if success, false if not enough space
 */
bool buffer_write_u8(write_buffer_t *buffer, uint8_t value);

/**
 * Write 2 bytes to buffer with specified endianness.
 *
 * @param[in,out] buffer Pointer to buffer struct
 * @param[in] value Value to write
 * @param[in] endianness BE or LE
 * @return true if success, false if not enough space
 */
bool buffer_write_u16(write_buffer_t *buffer, uint16_t value, endianness_t endianness);

/**
 * Write 4 bytes to buffer with specified endianness.
 *
 * @param[in,out] buffer Pointer to buffer struct
 * @param[in] value Value to write
 * @param[in] endianness BE or LE
 * @return true if success, false if not enough space
 */
bool buffer_write_u32(write_buffer_t *buffer, uint32_t value, endianness_t endianness);

/**
 * Write 8 bytes to buffer with specified endianness.
 *
 * @param[in,out] buffer Pointer to buffer struct
 * @param[in] value Value to write
 * @param[in] endianness BE or LE
 * @return true if success, false if not enough space
 */
bool buffer_write_u64(write_buffer_t *buffer, uint64_t value, endianness_t endianness);

/**
 * Write n bytes to buffer from source array.
 *
 * @param[in,out] buffer Pointer to buffer struct
 * @param[in] data Source data to copy
 * @param[in] n Number of bytes to copy
 * @return true if success, false if not enough space
 */
bool buffer_write_bytes(write_buffer_t *buffer, const uint8_t *data, size_t n);

/**
 * Write a CBOR token to buffer.
 * Wraps cbor_writeToken with buffer safety checks.
 *
 * @param[in,out] buffer Pointer to buffer struct
 * @param[in] type CBOR type tag
 * @param[in] value CBOR value
 * @return true if success, false if not enough space
 */
bool buffer_write_cbor_token(write_buffer_t *buffer, uint8_t type, uint64_t value);

/**
 * Read n bytes from buffer without copying.
 * Sets destBuffer to point to the beginning of the data in the buffer,
 * and advances buffer position by n bytes.
 *
 * @param[in,out] buffer Pointer to read buffer struct
 * @param[out] destBuffer Pointer to be set to data location in buffer
 * @param[in] n Number of bytes to reference
 * @return true if success, false if not enough data available
 */
bool buffer_read_bytes_ptr(buffer_t *buffer, uint8_t **destBuffer, size_t n);
