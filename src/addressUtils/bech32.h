#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define BECH32_BUFFER_SIZE_MAX   150
#define BECH32_PREFIX_LENGTH_MAX 16
#define BECH32_STRING_SIZE_MAX   (1 + 11 + BECH32_PREFIX_LENGTH_MAX + 2 * BECH32_BUFFER_SIZE_MAX)

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
