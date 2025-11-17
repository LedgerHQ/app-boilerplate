// CRC32 mock implementation for unit testing
// Based on reference implementation from zlib

#include <stdint.h>
#include <stddef.h>

// CRC32 lookup table
static const uint32_t crc32_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

// Compute CRC32 checksum
// crc should be CX_CRC32_INIT (0xFFFFFFFF) for initial computation
uint32_t cx_crc32(const void *buf, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    const unsigned char *data = (const unsigned char *) buf;

    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        crc = crc32_table[crc & 0x0f] ^ (crc >> 4);
        crc = crc32_table[crc & 0x0f] ^ (crc >> 4);
    }

    return crc ^ 0xffffffff;
}

// Accumulate CRC32 with existing state
uint32_t cx_crc32_update(uint32_t crc_state, const void *buf, size_t len) {
    const unsigned char *data = (const unsigned char *) buf;

    for (size_t i = 0; i < len; ++i) {
        crc_state ^= data[i];
        crc_state = crc32_table[crc_state & 0x0f] ^ (crc_state >> 4);
        crc_state = crc32_table[crc_state & 0x0f] ^ (crc_state >> 4);
    }

    return crc_state;
}
