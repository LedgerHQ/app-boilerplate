#include "utils/assert.h"
#include "hexUtils.h"
#include <string.h>

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

// Helper: check if character is a separator (space, tab, newline, carriage return, underscore)
static bool is_hex_separator(char c) {
    return c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '_';
}

size_t hex_to_bytes(const char* hex, uint8_t* out, size_t max_size) {
    // Count non-separator hex digits
    size_t digits = 0;
    for (const char *p = hex; *p != '\0'; p++) {
        if (!is_hex_separator(*p)) {
            digits++;
        }
    }

    // Verify even number of hex digits
    LEDGER_ASSERT((digits % 2) == 0, "hex_to_bytes: odd number of hex digits");

    size_t out_len = digits / 2;
    LEDGER_ASSERT(out_len <= max_size, "hex_to_bytes: output buffer too small");

    // Normalize hex string by removing separators
    char* normalized = (char*) malloc(digits + 1);
    LEDGER_ASSERT(normalized != NULL, "hex_to_bytes: malloc failed");

    size_t idx = 0;
    for (const char *p = hex; *p != '\0'; p++) {
        if (!is_hex_separator(*p)) {
            normalized[idx++] = *p;
        }
    }
    normalized[idx] = '\0';

    // Parse normalized hex string into bytes
    for (size_t i = 0; i < out_len; i++) {
        LEDGER_ASSERT(hex_parseNibblePair(&normalized[2 * i], &out[i]),
                      "hex_to_bytes: invalid hex character");
    }

    free(normalized);
    return out_len;
}

// Test utility: encode bytes to lowercase hex (for testing purposes)
// Returns 0 on success, -1 if buffer too small (matching SDK's bytes_to_lowercase_hex behavior)
int test_bytes_to_lowercase_hex(char* out, size_t outl, const uint8_t* bytes, size_t bytesLength) {
    const char* hex = "0123456789abcdef";

    if (outl < 2 * bytesLength + 1) {
        if (outl > 0) *out = '\0';
        return -1;
    }

    for (size_t i = 0; i < bytesLength; i++) {
        *out++ = hex[(bytes[i] >> 4) & 0xf];
        *out++ = hex[bytes[i] & 0xf];
    }
    *out = 0;
    return 0;
}

