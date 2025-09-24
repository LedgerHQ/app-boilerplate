#pragma once

#include "utils/utils.h"

uint64_t abs_int64(int64_t number);

size_t str_formatDecimalAmount(uint64_t amount, size_t places, char* out, size_t outSize);
size_t str_formatAdaAmount(uint64_t amount, char* out, size_t outSize);

size_t str_formatUint64(uint64_t number, char* out, size_t outSize);
size_t str_formatInt64(int64_t number, char* out, size_t outSize);

#ifdef DEVEL
void str_traceAdaAmount(const char* prefix, uint64_t amount);
#define TRACE_ADA_AMOUNT(PREFIX, AMOUNT)    \
    do {                                    \
        str_traceAdaAmount(PREFIX, AMOUNT); \
    } while (0)
#else
#define TRACE_ADA_AMOUNT(PREFIX, AMOUNT)
#endif  // DEVEL

#ifdef DEVEL
void str_traceUint64(uint64_t number);
#define TRACE_UINT64(NUMBER)     \
    do {                         \
        str_traceUint64(NUMBER); \
    } while (0)
#else
#define TRACE_UINT64(NUMBER)
#endif  // DEVEL

#ifdef DEVEL
void str_traceInt64(int64_t number);
#define TRACE_INT64(NUMBER)     \
    do {                        \
        str_traceInt64(NUMBER); \
    } while (0)
#else
#define TRACE_INT64(NUMBER)
#endif  // DEVEL

size_t str_formatValidityBoundary(uint64_t slotNumber, char* out, size_t outSize);

bool str_isPrintableAsciiWithoutSpaces(const uint8_t* buffer, size_t bufferSize);
bool str_isPrintableAsciiWithSpaces(const uint8_t* buffer, size_t bufferSize);

bool str_isUnambiguousAscii(const uint8_t* buffer, size_t bufferSize);

#ifdef DEVEL
size_t str_textToBuffer(const char* text, uint8_t* buffer, size_t bufferSize);
#endif  // DEVEL
