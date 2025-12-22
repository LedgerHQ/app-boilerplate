#include "utils/assert.h"
#include "utils/utils.h"
#include "textUtils.h"
#include "utils/ipUtils.h"
#include "cardano_constants.h"
#include <string.h>
#include <stdint.h>

#define WRITE_CHAR(ptr, end, c) \
    {                           \
        ASSERT(ptr + 1 <= end); \
        *ptr = (c);             \
        ptr++;                  \
    }

uint64_t abs_int64(int64_t number) {
    // INT64_MIN cannot be negated safely, so handle it specially
    if (number == INT64_MIN) {
        return (uint64_t)INT64_MAX + 1;
    }
    return (uint64_t)(number < 0 ? -number : number);
}

bool str_formatDecimalAmount(uint64_t amount, size_t places, char* out, size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);
    ASSERT(places <= UINT8_MAX);

    explicit_bzero(out, outSize);

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

    return true;
}

bool str_formatAdaAmount(uint64_t amount, char* out, size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    explicit_bzero(out, outSize);

    // TODO: Consider using format_fpu64 directly instead of str_formatDecimalAmount
    // for consistency with other formatting code
    bool formatted = str_formatDecimalAmount(amount, 6, out, outSize);
    ASSERT(formatted);
    const size_t rawSize = strlen(out);

    const char* suffix = " ADA";
    const size_t suffixLength = strlen(suffix);

    // make sure all the information is displayed to the user
    ASSERT(rawSize + suffixLength + 1 < outSize);

    snprintf(out + rawSize, outSize - rawSize, "%s", suffix);
    ASSERT(strlen(out) == rawSize + suffixLength);

    return true;
}


#ifdef DEVEL
void str_traceAdaAmount(const char* prefix, uint64_t amount) {
    char adaAmountStr[100] = {0};
    explicit_bzero(adaAmountStr, SIZEOF(adaAmountStr));

    const size_t prefixLen = strlen(prefix);
    ASSERT(prefixLen <= 50);
    snprintf(adaAmountStr, SIZEOF(adaAmountStr), "%s", prefix);
    ASSERT(strlen(adaAmountStr) == prefixLen);

    bool formatted = str_formatAdaAmount(amount, adaAmountStr + prefixLen, SIZEOF(adaAmountStr) - prefixLen);
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
#endif  // DEVEL

// Note: This is valid only for mainnet
static struct {
    uint64_t startSlotNumber;
    uint64_t startEpoch;
    uint64_t slotsInEpoch;
} EPOCH_SLOTS_CONFIG[] = {{4492800, 208, 432000}, {0, 0, 21600}};

bool str_formatValidityBoundaryMainnet(uint64_t slotNumber, char* out, size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    explicit_bzero(out, outSize);

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

    return true;
}

bool str_formatValidityBoundary(uint64_t slotNumber,
                                uint8_t networkId,
                                uint32_t protocolMagic,
                                char* out,
                                size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);

    explicit_bzero(out, outSize);

    // Determine if we can use the nicer mainnet formatting
    // Note: Epoch/slot calculations are valid only for mainnet,
    // as they depend on network params that could differ for testnets
    if ((networkId == MAINNET_NETWORK_ID) && (protocolMagic == MAINNET_PROTOCOL_MAGIC)) {
        // Use pretty formatting for mainnet (epoch / slot)
        return str_formatValidityBoundaryMainnet(slotNumber, out, outSize);
    } else {
        // Use simple uint64 formatting for non-mainnet
        bool success = format_u64(out, outSize, slotNumber);
        ASSERT(success);
        size_t len = strlen(out);
        ASSERT(len + 1 < outSize);
        return true;
    }
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

bool str_formatIpv4(const ipv4_t* ipv4, char* out, size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);
    ASSERT(out != NULL);
    ASSERT(ipv4 != NULL);

    explicit_bzero(out, outSize);

    if (ipv4->isNull) {
        snprintf(out, outSize, "(none)");
    } else {
        inet_ntop4(ipv4->ip, out, outSize);
    }

    // make sure all the information is displayed to the user
    ASSERT(strlen(out) + 1 < outSize);

    return true;
}

bool str_formatIpv6(const ipv6_t* ipv6, char* out, size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);
    ASSERT(out != NULL);
    ASSERT(ipv6 != NULL);

    explicit_bzero(out, outSize);

    if (ipv6->isNull) {
        snprintf(out, outSize, "(none)");
    } else {
        inet_ntop6(ipv6->ip, out, outSize);
    }

    // make sure all the information is displayed to the user
    ASSERT(strlen(out) + 1 < outSize);

    return true;
}

bool str_formatIpPort(const ipport_t* port, char* out, size_t outSize) {
    ASSERT(outSize < BUFFER_SIZE_PARANOIA);
    ASSERT(out != NULL);
    ASSERT(port != NULL);

    explicit_bzero(out, outSize);

    if (port->isNull) {
        snprintf(out, outSize, "(none)");
    } else {
        STATIC_ASSERT(sizeof(port->number) <= sizeof(unsigned), "oversized variable for %u");
        STATIC_ASSERT(!IS_SIGNED(port->number), "signed type for %u");
        snprintf(out, outSize, "%u", port->number);
    }

    // make sure all the information is displayed to the user
    ASSERT(strlen(out) + 1 < outSize);

    return true;
}
