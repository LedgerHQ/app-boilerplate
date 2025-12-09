#pragma once

#include "utils/utils.h"

// Parse a single hex nibble character
// Returns true on success and sets *out_nibble
// Returns false on any error (invalid hex character)
bool hex_parseNibble(const char c, uint8_t* out_nibble);

// Parse two consecutive hex nibble characters into a single byte
// Returns true on success and sets *out_byte
// Returns false on any error (invalid hex characters)
bool hex_parseNibblePair(const char* buffer, uint8_t* out_byte);

// Decode a hex string into binary buffer
// Returns true on success and sets *out_length
// Returns false on any error (invalid hex characters, odd length, buffer too small)
bool decode_hex(const char* inStr, uint8_t* outBuffer, size_t outMaxSize, size_t* out_length);

// Decode a hex string with optional separators (spaces, underscores, newlines, tabs) into binary buffer
// Useful for readable test fixtures where hex strings may contain formatting
// Returns the number of bytes decoded, or 0 on error
// Note: Uses assertions on error (for test code)
size_t hex_to_bytes(const char* hex, uint8_t* out, size_t max_size);

// Test utility: encode bytes to lowercase hex (for testing purposes)
// Returns 0 on success, -1 if buffer too small (matching SDK's bytes_to_lowercase_hex behavior)
int test_bytes_to_lowercase_hex(char* out, size_t outl, const uint8_t* bytes, size_t bytesLength);
