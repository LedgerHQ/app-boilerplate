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

// Encode binary buffer into hex string
// Returns the length of the string written to out (not including null terminator)
size_t encode_hex(const uint8_t* bytes, size_t bytesLength, char* outString, size_t outMaxSize);
