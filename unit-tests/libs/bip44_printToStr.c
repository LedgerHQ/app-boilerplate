// Minimal bip44_printToStr implementation extracted for testing
// This is a standalone version to avoid complex crypto dependencies

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "utils/assert.h"
#include "utils/utils.h"
#include "addressUtils/bip44.h"

#define HARDENED_BIP32 0x80000000

// Copy of bip44_printToStr from ../src/addressUtils/bip44.c
// Extracted here to avoid including hash.h and crypto dependencies
size_t bip44_printToStr(const bip44_path_t* pathSpec, char* out, size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);
    // we need space for the terminating \0
    // and one more byte to check whether
    // everything was printed
    ASSERT(outSize >= BIP44_PATH_STRING_SIZE_MAX + 1);
    char* ptr = out;
    char* end = (out + outSize);

#define WRITE(fmt, ...)                                                               \
    {                                                                                 \
        ASSERT(ptr <= end);                                                           \
        size_t availableSize = (size_t)(end - ptr);                                   \
        snprintf(ptr, availableSize, fmt, ##__VA_ARGS__);                             \
        size_t res = strlen(ptr);                                                     \
        if (res >= availableSize - 1) {                                               \
            ASSERT(!"not enough space for bip44 string");                             \
        }                                                                              \
        ptr += res;                                                                   \
    }

    WRITE("m");
    for (size_t i = 0; i < pathSpec->length; i++) {
        uint32_t index = pathSpec->path[i];
        if (index >= HARDENED_BIP32) {
            WRITE("/%u'", index - HARDENED_BIP32);
        } else {
            WRITE("/%u", index);
        }
    }

    size_t result = ptr - out;
    ASSERT(result < outSize);
    ASSERT(out[result] == '\0');
    return result;

#undef WRITE
}
