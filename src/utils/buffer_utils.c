#include "buffer_utils.h"
#include "buffer.h" // for buffer_* functions from SDK
#include "write.h"  // for write_u16_be, write_u32_be, write_u64_be, write_u16_le, write_u32_le, write_u64_le
#include "cbor.h"   // for cbor_writeToken
#include "assert.h"
#include <string.h> // for memmove, memcpy

// Seek forward in write buffer (internal helper, distinct from SDK's buffer_seek_cur)
static bool write_buffer_seek_cur(write_buffer_t *buffer, size_t offset) {
    if (buffer->offset + offset < buffer->offset ||  // overflow check
        buffer->offset + offset > buffer->size) {    // bounds check
        return false;
    }
    buffer->offset += offset;
    return true;
}

bool buffer_write_u8(write_buffer_t *buffer, uint8_t value) {
    if (!buffer_can_write(buffer, 1)) {
        return false;
    }
    buffer->ptr[buffer->offset] = value;
    return write_buffer_seek_cur(buffer, 1);
}

bool buffer_write_u16(write_buffer_t *buffer, uint16_t value, endianness_t endianness) {
    if (!buffer_can_write(buffer, 2)) {
        return false;
    }
    if (endianness == BE) {
        write_u16_be(buffer->ptr, buffer->offset, value);
    } else {
        write_u16_le(buffer->ptr, buffer->offset, value);
    }
    return write_buffer_seek_cur(buffer, 2);
}

bool buffer_write_u32(write_buffer_t *buffer, uint32_t value, endianness_t endianness) {
    if (!buffer_can_write(buffer, 4)) {
        return false;
    }
    if (endianness == BE) {
        write_u32_be(buffer->ptr, buffer->offset, value);
    } else {
        write_u32_le(buffer->ptr, buffer->offset, value);
    }
    return write_buffer_seek_cur(buffer, 4);
}

bool buffer_write_u64(write_buffer_t *buffer, uint64_t value, endianness_t endianness) {
    if (!buffer_can_write(buffer, 8)) {
        return false;
    }
    if (endianness == BE) {
        write_u64_be(buffer->ptr, buffer->offset, value);
    } else {
        write_u64_le(buffer->ptr, buffer->offset, value);
    }
    return write_buffer_seek_cur(buffer, 8);
}

bool buffer_write_bytes(write_buffer_t *buffer, const uint8_t *data, size_t n) {
    if (!buffer_can_write(buffer, n)) {
        return false;
    }
    memmove(buffer->ptr + buffer->offset, data, n);
    return write_buffer_seek_cur(buffer, n);
}

bool buffer_write_cbor_token(write_buffer_t *buffer, uint8_t type, uint64_t value) {
    ASSERT(buffer_remaining_size(buffer) <= BUFFER_SIZE_PARANOIA);

    if (!buffer_can_write(buffer, 1)) {
        return false;
    }

    size_t written = 0;
    if (!cbor_writeToken(type, value,
                         buffer->ptr + buffer->offset,
                         buffer_remaining_size(buffer),
                         &written)) {
        // cbor_writeToken returns false on failure
        return false;
    }

    return write_buffer_seek_cur(buffer, written);
}

bool buffer_read_bytes(buffer_t *buffer, uint8_t *destBuffer, size_t n) {
    LEDGER_ASSERT(buffer != NULL, "NULL buffer");
    LEDGER_ASSERT(buffer->ptr != NULL, "NULL buffer ptr");
    LEDGER_ASSERT(destBuffer != NULL, "NULL destination");

    if (!buffer_can_read(buffer, n)) {
        return false;
    }

    memmove(destBuffer, buffer->ptr + buffer->offset, n);
    return buffer_seek_cur(buffer, n);
}

bool buffer_read_bytes_ptr(buffer_t *buffer, uint8_t **destBuffer, size_t n) {
    LEDGER_ASSERT(buffer != NULL, "NULL buffer");

    *destBuffer = (uint8_t *)(buffer->ptr + buffer->offset);
    if (!buffer_seek_cur(buffer, n)) {
        return false;
    }
    return true;
}
