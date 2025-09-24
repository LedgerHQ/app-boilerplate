#include "textUtils.h"
#include "hexUtils.h"

#define WRITE_CHAR(ptr, end, c) \
    {                           \
        ASSERT(ptr + 1 <= end); \
        *ptr = (c);             \
        ptr++;                  \
    }

size_t str_formatDecimalAmount(uint64_t amount, size_t places, char* out, size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);
    ASSERT(places <= UINT8_MAX);

    char scratchBuffer[40] = {0};
    explicit_bzero(scratchBuffer, SIZEOF(scratchBuffer));
    char* ptr = scratchBuffer;
    char* end = scratchBuffer + SIZEOF(scratchBuffer);

    // We print in reverse

    // decimal digits
    for (size_t dec = 0; dec < places; dec++) {
        WRITE_CHAR(ptr, end, '0' + (amount % 10));
        amount /= 10;
    }
    if (places > 0) {
        WRITE_CHAR(ptr, end, '.');
    }
    // We want at least one iteration
    int place = 0;
    do {
        // thousands separator
        if (place && (place % 3 == 0)) {
            WRITE_CHAR(ptr, end, ',');
        }
        WRITE_CHAR(ptr, end, '0' + (amount % 10));
        amount /= 10;
        place++;
    } while (amount > 0);

    // Size without terminating character
    STATIC_ASSERT(sizeof(ptr - scratchBuffer) == sizeof(size_t), "bad size_t size");
    size_t rawSize = (size_t)(ptr - scratchBuffer);
    ASSERT(rawSize + 1 <= outSize);

    // Copy reversed & append terminator
    for (size_t i = 0; i < rawSize; i++) {
        out[i] = scratchBuffer[rawSize - 1 - i];
    }
    out[rawSize] = 0;

    // make sure all the information is displayed to the user
    ASSERT(strlen(out) == rawSize);

    return rawSize;
}

size_t str_formatAdaAmount(uint64_t amount, char* out, size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    size_t rawSize = str_formatDecimalAmount(amount, 6, out, outSize);

    const char* suffix = " ADA";
    const size_t suffixLength = strlen(suffix);

    // make sure all the information is displayed to the user
    ASSERT(rawSize + suffixLength + 1 < outSize);

    snprintf(out + rawSize, outSize - rawSize, "%s", suffix);
    ASSERT(strlen(out) == rawSize + suffixLength);

    return rawSize + suffixLength;
}

static size_t stringifyUint64ToBufferReverse(uint64_t number, char* buffer, size_t bufferSize) {
    char* currChar = buffer;
    char* const end = buffer + bufferSize;

    // We print in reverse
    // We want at least one iteration
    size_t printedChars = 0;
    do {
        WRITE_CHAR(currChar, end, '0' + (number % 10));
        number /= 10;
        ++printedChars;
    } while (number > 0);
    WRITE_CHAR(currChar, end, '\0');
    return printedChars;
}

static void printReversedStringToBuffer(const char* reversed, char* out, size_t outSize) {
    const size_t reversedSize = strlen(reversed);
    ASSERT(outSize >= reversedSize + 1);

    for (size_t i = 0; i < reversedSize; i++) {
        out[i] = reversed[reversedSize - 1 - i];
    }
    out[reversedSize] = 0;

    ASSERT(strlen(out) == reversedSize);
}

size_t str_formatUint64(uint64_t number, char* out, size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    {
        char tmpReversed[30] = {0};
        explicit_bzero(tmpReversed, SIZEOF(tmpReversed));

        stringifyUint64ToBufferReverse(number, tmpReversed, SIZEOF(tmpReversed));
        const size_t reversedSize = strlen(tmpReversed);
        printReversedStringToBuffer(tmpReversed, out, outSize);
        ASSERT(strlen(out) == reversedSize);
    }
    return strlen(out);
}

uint64_t abs_int64(int64_t number) {
    if (number < 0) {
        if (number == INT64_MIN) {
            return ((uint64_t) INT64_MAX) + 1;
        } else {
            return (uint64_t)(-number);
        }
    } else {
        return (uint64_t) number;
    }
}

size_t str_formatInt64(int64_t number, char* out, size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    const uint64_t signlessNumber = abs_int64(number);
    {
        char tmpReversed[30] = {0};
        explicit_bzero(tmpReversed, SIZEOF(tmpReversed));
        // size without the potential '-' sign
        stringifyUint64ToBufferReverse(signlessNumber, tmpReversed, SIZEOF(tmpReversed) - 1);
        size_t reversedLength = strlen(tmpReversed);
        if (number < 0) {
            snprintf(tmpReversed + reversedLength, SIZEOF(tmpReversed) - reversedLength, "%c", '-');
            ++reversedLength;
        }
        printReversedStringToBuffer(tmpReversed, out, outSize);
        ASSERT(strlen(out) == reversedLength);
    }
    return strlen(out);
}

#ifdef DEVEL
void str_traceAdaAmount(const char* prefix, uint64_t amount) {
    char adaAmountStr[100] = {0};
    explicit_bzero(adaAmountStr, SIZEOF(adaAmountStr));

    const size_t prefixLen = strlen(prefix);
    ASSERT(prefixLen <= 50);
    snprintf(adaAmountStr, SIZEOF(adaAmountStr), "%s", prefix);
    ASSERT(strlen(adaAmountStr) == prefixLen);

    str_formatAdaAmount(amount, adaAmountStr + prefixLen, SIZEOF(adaAmountStr) - prefixLen);
    TRACE("%s", adaAmountStr);
}

void str_traceUint64(uint64_t number) {
    char numberStr[30] = {0};
    explicit_bzero(numberStr, SIZEOF(numberStr));

    str_formatUint64(number, numberStr, SIZEOF(numberStr));
    TRACE("%s", numberStr);
}

void str_traceInt64(int64_t number) {
    char numberStr[30] = {0};
    explicit_bzero(numberStr, SIZEOF(numberStr));

    str_formatInt64(number, numberStr, SIZEOF(numberStr));
    TRACE("%s", numberStr);
}
#endif  // DEVEL

// Note: This is valid only for mainnet
static struct {
    uint64_t startSlotNumber;
    uint64_t startEpoch;
    uint64_t slotsInEpoch;
} EPOCH_SLOTS_CONFIG[] = {{4492800, 208, 432000}, {0, 0, 21600}};

size_t str_formatValidityBoundary(uint64_t slotNumber, char* out, size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    unsigned i = 0;
    while (slotNumber < EPOCH_SLOTS_CONFIG[i].startSlotNumber) {
        i++;
        ASSERT(i < ARRAY_LEN(EPOCH_SLOTS_CONFIG));
    }

    ASSERT(slotNumber >= EPOCH_SLOTS_CONFIG[i].startSlotNumber);

    uint64_t startSlotNumber = EPOCH_SLOTS_CONFIG[i].startSlotNumber;
    uint64_t startEpoch = EPOCH_SLOTS_CONFIG[i].startEpoch;
    uint64_t slotsInEpoch = EPOCH_SLOTS_CONFIG[i].slotsInEpoch;

    uint64_t epoch = startEpoch + (slotNumber - startSlotNumber) / slotsInEpoch;
    uint64_t slotInEpoch = (slotNumber - startSlotNumber) % slotsInEpoch;

    STATIC_ASSERT(sizeof(int) >= sizeof(uint32_t), "wrong int size");

    ASSERT(outSize > 0);  // so we can write null terminator
    if (epoch > 1000000) {
        // thousands of years
        snprintf(out, outSize, "epoch more than 1000000");
    } else {
        snprintf(out, outSize, "epoch %u / slot %u", (unsigned) epoch, (unsigned) slotInEpoch);
    }

    // snprintf does not return length written
    size_t len = strlen(out);
    // make sure we did not truncate
    ASSERT(len + 1 < outSize);

    return strlen(out);
}

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

#ifdef DEVEL

/* cspell:disable-next-line */
// converts a text to bytes (suitable for CBORization) and validates if chars are allowed
size_t str_textToBuffer(const char* text, uint8_t* buffer, size_t bufferSize) {
    size_t textLength = strlen(text);
    ASSERT(textLength < BUFFER_SIZE_PARANOIA);
    ASSERT(bufferSize < BUFFER_SIZE_PARANOIA);
    ASSERT(bufferSize >= textLength);

    for (size_t i = 0; i < textLength; i++) {
        buffer[i] = text[i];
    }

    ASSERT(str_isPrintableAsciiWithSpaces(buffer, textLength));

    return textLength;
}

#endif  // DEVEL
