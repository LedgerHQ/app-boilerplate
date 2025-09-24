#include "utils/utils.h"
#include "sw.h"

uint8_t hex_parseNibble(const char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    THROW(ERR_UNEXPECTED_TOKEN);
}

uint8_t hex_parseNibblePair(const char* buffer) {
    uint8_t first = hex_parseNibble(buffer[0]);
    uint8_t second = hex_parseNibble(buffer[1]);
    return (uint8_t)((first << 4) + second);
}

size_t decode_hex(const char* inStr, uint8_t* outBuffer, size_t outMaxSize) {
    ASSERT(outMaxSize < BUFFER_SIZE_PARANOIA);

    size_t len = strlen(inStr);
    ASSERT(len % 2 == 0);

    size_t outLen = len / 2;
    ASSERT(outLen <= outMaxSize);

    while (len >= 2) {
        *outBuffer = hex_parseNibblePair(inStr);
        len -= 2;
        inStr += 2;
        outBuffer += 1;
    }
    return outLen;
}

static const char HEX_ALPHABET[] = "0123456789abcdef";

// returns the length of the string written to out
size_t encode_hex(const uint8_t* bytes, size_t bytesLength, char* out, size_t outMaxSize) {
    ASSERT(bytesLength < BUFFER_SIZE_PARANOIA);
    ASSERT(outMaxSize < BUFFER_SIZE_PARANOIA);
    ASSERT(outMaxSize >= 2 * bytesLength + 1);

    size_t i = 0;
    for (; i < bytesLength; i++) {
        out[2 * i] = HEX_ALPHABET[bytes[i] >> 4];
        out[2 * i + 1] = HEX_ALPHABET[bytes[i] & 0x0F];
    }
    ASSERT(i == bytesLength);
    out[2 * i] = '\0';

    return 2 * bytesLength;
}
