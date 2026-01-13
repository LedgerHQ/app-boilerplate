#include "utils/assert.h"
#include "utils/utils.h"
#include "textUtils.h"
#include "ui/ui_formatters.h"
#include <string.h>
#include <stdint.h>

#ifdef DEBUG
void str_traceAdaAmount(const char* prefix, uint64_t amount) {
    char adaAmountStr[100] = {0};
    explicit_bzero(adaAmountStr, SIZEOF(adaAmountStr));

    const size_t prefixLen = strlen(prefix);
    ASSERT(prefixLen <= 50);
    snprintf(adaAmountStr, SIZEOF(adaAmountStr), "%s", prefix);
    ASSERT(strlen(adaAmountStr) == prefixLen);

    bool formatted = format_ada_amount(amount, adaAmountStr + prefixLen, SIZEOF(adaAmountStr) - prefixLen);
    ASSERT(formatted);
    TRACE("%s", adaAmountStr);
}

void str_traceUint64(uint64_t number) {
    char numberStr[30] = {0};
    explicit_bzero(numberStr, SIZEOF(numberStr));

    format_u64(numberStr, SIZEOF(numberStr), number);
    TRACE("%s", numberStr);
}

void str_traceInt64(int64_t number) {
    char numberStr[30] = {0};
    explicit_bzero(numberStr, SIZEOF(numberStr));

    format_i64(numberStr, SIZEOF(numberStr), number);
    TRACE("%s", numberStr);
}
#endif  // DEBUG

// check if a non-null-terminated buffer contains printable ASCII between 33 and 126 (inclusive)
bool str_isPrintableAsciiWithoutSpaces(const uint8_t* buffer, size_t bufferSize) {
    ASSERT(bufferSize < BUFFER_SIZE_PARANOIA);

    for (size_t i = 0; i < bufferSize; i++) {
        if (buffer[i] > 126) return false;
        if (buffer[i] < 33) return false;
    }

    return true;
}

// check if a non-null-terminated buffer contains printable ASCII between 32 and 126 (inclusive)
bool str_isPrintableAsciiWithSpaces(const uint8_t* buffer, size_t bufferSize) {
    ASSERT(bufferSize < BUFFER_SIZE_PARANOIA);

    for (size_t i = 0; i < bufferSize; i++) {
        if (buffer[i] > 126) return false;
        if (buffer[i] < 32) return false;
    }

    return true;
}

// check if the string can be unambiguously displayed to the user
bool str_isUnambiguousAscii(const uint8_t* buffer, size_t bufferSize) {
    ASSERT(bufferSize < BUFFER_SIZE_PARANOIA);

    // must not be empty
    if (bufferSize == 0) return false;

    // no non-printable characters except spaces
    if (!str_isPrintableAsciiWithSpaces(buffer, bufferSize)) return false;

    // no leading spaces
    ASSERT(bufferSize >= 1);
    if (buffer[0] == ' ') return false;

    // no trailing spaces
    ASSERT(bufferSize >= 1);
    if (buffer[bufferSize - 1] == ' ') return false;

    // only single spaces
    for (size_t i = 0; i + 1 < bufferSize; i++) {
        if ((buffer[i] == ' ') && (buffer[i + 1] == ' ')) return false;
    }

    return true;
}
