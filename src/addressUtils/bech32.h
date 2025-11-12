#pragma once

#include <stdint.h>
#include <stddef.h>

#define BECH32_BUFFER_SIZE_MAX   150
#define BECH32_PREFIX_LENGTH_MAX 16
#define BECH32_STRING_SIZE_MAX   (1 + 11 + BECH32_PREFIX_LENGTH_MAX + 2 * BECH32_BUFFER_SIZE_MAX)

/*
 * Encode bytes, using human-readable prefix given in hrp.
 *
 * The return value is the length of the resulting bech32-encoded string,
 * i.e. strlen(hrp) + 1 [separator] + 6 [checksum] +
       + ceiling of (8/5 * bytesLen) [base32 encoding with padding].

 * The output buffer must be capable of storing one more character.
 */
size_t bech32_encode(const char* hrp,
                     const uint8_t* bytes,
                     size_t bytesSize,
                     char* output,
                     size_t maxOutputSize);
