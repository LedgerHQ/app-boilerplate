#pragma once

#include "utils/utils.h"

uint8_t hex_parseNibble(const char c);

uint8_t hex_parseNibblePair(const char* buffer);

size_t decode_hex(const char* inStr, uint8_t* outBuffer, size_t outMaxSize);

size_t encode_hex(const uint8_t* bytes, size_t bytesLength, char* outString, size_t outMaxSize);
