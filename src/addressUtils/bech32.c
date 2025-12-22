/* Copyright (c) 2017 Pieter Wuille
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// modified by Vacuumlabs

#include <string.h>
#include "utils/assert.h"
#include "utils/utils.h"
#include "bech32.h"

uint32_t bech32_polymod_step(uint32_t pre) {
    const uint8_t b = pre >> 25;
    return ((pre & 0x1FFFFFF) << 5) ^ (-((b >> 0) & 1) & 0x3b6a57b2UL) ^
           (-((b >> 1) & 1) & 0x26508e6dUL) ^ (-((b >> 2) & 1) & 0x1ea119faUL) ^
           (-((b >> 3) & 1) & 0x3d4233ddUL) ^ (-((b >> 4) & 1) & 0x2a1462b3UL);
}

/* cspell:disable-next-line */
static const char* charset = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

/** Encode a Bech32 string
 *
 *  Out: output:  Pointer to a buffer of size >= strlen(hrp) + data_len + 8 that
 *                will be updated to contain the null-terminated Bech32 string.
 *  In: hrp :     Pointer to the null-terminated human readable part.
 *      data :    Pointer to an array of 5-bit values.
 *      data_len: Length of the data array.
 */
void bech32_encode_5bit(const char* hrp,
                        const uint8_t* data,
                        size_t data_len,
                        char* output,
                        size_t maxOutputSize) {
    ASSERT(maxOutputSize < BUFFER_SIZE_PARANOIA);
    ASSERT(maxOutputSize >= strlen(hrp) + data_len + 8);
    const char* outputLimit = output + maxOutputSize;

#define APPEND_OUT(value)             \
    {                                 \
        ASSERT(output < outputLimit); \
        *(output++) = (value);        \
    }

    uint32_t chk = 1;
    {
        size_t i = 0;
        while (hrp[i] != 0) {
            ASSERT(!(hrp[i] >= 'A' && hrp[i] <= 'Z'));
            ASSERT((hrp[i] >> 5) != 0);
            chk = bech32_polymod_step(chk) ^ (hrp[i] >> 5);
            ++i;
        }
    }
    chk = bech32_polymod_step(chk);
    while (*hrp != 0) {
        chk = bech32_polymod_step(chk) ^ (*hrp & 0x1f);
        APPEND_OUT(*(hrp++));
    }

    APPEND_OUT('1');

    for (size_t i = 0; i < data_len; ++i) {
        ASSERT((*data >> 5) == 0);
        chk = bech32_polymod_step(chk) ^ (*data);
        APPEND_OUT(charset[*(data++)]);
    }
    for (size_t i = 0; i < 6; ++i) {
        chk = bech32_polymod_step(chk);
    }
    chk ^= 1;
    for (size_t i = 0; i < 6; ++i) {
        APPEND_OUT(charset[(chk >> ((5 - i) * 5)) & 0x1f]);
    }
    APPEND_OUT(0);
}

// we are not supposed to use more for Cardano Shelley
// WARNING: increasing this would take more stack space, see data5bit definition below
#define MAX_BECH32_BYTES_LENGTH 65

bool format_bech32(const char* hrp,
                   const uint8_t* bytes,
                   size_t bytesSize,
                   char* output,
                   size_t maxOutputSize) {
    ASSERT(bytesSize <= MAX_BECH32_BYTES_LENGTH);
    ASSERT(strlen(hrp) >= 1);  // not allowed for bech32

    const size_t SEPARATOR_LEN = 1;
    const size_t CHECKSUM_LEN = 6;
    size_t ceiling =
        (8 * bytesSize + 4) / 5;  // ceiling of 8/5 * bytesLen (base32 encoding with padding)
    size_t supposedOutputLength = strlen(hrp) + SEPARATOR_LEN + ceiling + CHECKSUM_LEN;
    ASSERT(maxOutputSize >= supposedOutputLength + 1);
    ASSERT(maxOutputSize < BUFFER_SIZE_PARANOIA);
    ASSERT(bytesSize < BUFFER_SIZE_PARANOIA);

    uint8_t data5bit[(8 * MAX_BECH32_BYTES_LENGTH + 4) / 5] = {0};  // ceiling of (8/5 * MAX_BYTES_LEN) = 104
    size_t data5bitLength = 0;
    {
        const int OUTBITS = 5;
        const int INBITS = 8;

        // originally convert_bits()
        uint32_t val = 0;
        int bits = 0;
        uint32_t maxv = (((uint32_t) 1) << OUTBITS) - 1;
        while (bytesSize--) {
            val = (val << INBITS) | *(bytes++);
            bits += INBITS;
            while (bits >= OUTBITS) {
                bits -= OUTBITS;
                ASSERT(data5bitLength < SIZEOF(data5bit));
                data5bit[data5bitLength++] = (val >> bits) & maxv;
            }
        }
        if (bits) {
            ASSERT(data5bitLength < SIZEOF(data5bit));
            data5bit[data5bitLength++] = (val << (OUTBITS - bits)) & maxv;
        }
    }

    bech32_encode_5bit(hrp, data5bit, data5bitLength, output, maxOutputSize);
    ASSERT(strlen(output) == supposedOutputLength);

    return true;
}
