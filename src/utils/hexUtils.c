#include "utils/assert.h"
#include "hexUtils.h"

bool hex_parseNibble(const char c, uint8_t* out_nibble) {
    if (c >= '0' && c <= '9') {
        *out_nibble = c - '0';
        return true;
    }
    if (c >= 'a' && c <= 'f') {
        *out_nibble = c - 'a' + 10;
        return true;
    }
    if (c >= 'A' && c <= 'F') {
        *out_nibble = c - 'A' + 10;
        return true;
    }
    return false;
}

bool hex_parseNibblePair(const char* buffer, uint8_t* out_byte) {
    uint8_t first, second;
    if (!hex_parseNibble(buffer[0], &first)) {
        return false;
    }
    if (!hex_parseNibble(buffer[1], &second)) {
        return false;
    }
    *out_byte = (uint8_t)((first << 4) + second);
    return true;
}

bool decode_hex(const char* inStr, uint8_t* outBuffer, size_t outMaxSize, size_t* out_length) {
    LEDGER_ASSERT(outMaxSize < BUFFER_SIZE_PARANOIA, "outMaxSize too large");

    size_t len = strlen(inStr);
    if (len % 2 != 0) {
        return false;
    }

    size_t outLen = len / 2;
    if (outLen > outMaxSize) {
        return false;
    }

    uint8_t* write_ptr = outBuffer;
    const char* read_ptr = inStr;
    while (len >= 2) {
        if (!hex_parseNibblePair(read_ptr, write_ptr)) {
            return false;
        }
        len -= 2;
        read_ptr += 2;
        write_ptr += 1;
    }
    *out_length = outLen;
    return true;
}

static const char HEX_ALPHABET[] = "0123456789abcdef";

// returns the length of the string written to out
size_t encode_hex(const uint8_t* bytes, size_t bytesLength, char* out, size_t outMaxSize) {
    LEDGER_ASSERT(bytesLength < BUFFER_SIZE_PARANOIA, "bytesLength too large");
    LEDGER_ASSERT(outMaxSize < BUFFER_SIZE_PARANOIA, "outMaxSize too large");
    LEDGER_ASSERT(outMaxSize >= 2 * bytesLength + 1, "outMaxSize too small");

    size_t i = 0;
    for (; i < bytesLength; i++) {
        out[2 * i] = HEX_ALPHABET[bytes[i] >> 4];
        out[2 * i + 1] = HEX_ALPHABET[bytes[i] & 0x0F];
    }
    LEDGER_ASSERT(i == bytesLength, "loop counter mismatch");
    out[2 * i] = '\0';

    return 2 * bytesLength;
}
