#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_BECH32_BUFFER_LENGTH   150
#define MAX_BECH32_PREFIX_LENGTH 16
#define MAX_BECH32_STRING_LENGTH   (1 + 11 + MAX_BECH32_PREFIX_LENGTH + 2 * MAX_BECH32_BUFFER_LENGTH)

/*
 * Encode bytes, using human-readable prefix given in hrp.
 *
 * The resulting string length equals strlen(hrp) + 1 [separator] + 6 [checksum] +
 * ceiling(8/5 * bytesSize) [base32 encoding with padding], and the output buffer must
 * have space for the trailing null character.
 *
 * Returns true on success; formatting failures indicate bugs and should not happen in production.
 */
bool format_bech32(const char* hrp,
                     const uint8_t* bytes,
                     size_t bytesSize,
                     char* output,
                     size_t maxOutputSize);
