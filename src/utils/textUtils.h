#pragma once

#include "utils/utils.h"

#include "format.h"

#ifdef DEBUG
void str_traceAdaAmount(const char* prefix, uint64_t amount);
#define TRACE_ADA_AMOUNT(PREFIX, AMOUNT)    \
    do {                                    \
        str_traceAdaAmount(PREFIX, AMOUNT); \
    } while (0)
#else
#define TRACE_ADA_AMOUNT(PREFIX, AMOUNT)
#endif  // DEBUG

#ifdef DEBUG
void str_traceUint64(uint64_t number);
#define TRACE_UINT64(NUMBER)     \
    do {                         \
        str_traceUint64(NUMBER); \
    } while (0)
#else
#define TRACE_UINT64(NUMBER)
#endif  // DEBUG

#ifdef DEBUG
void str_traceInt64(int64_t number);
#define TRACE_INT64(NUMBER)     \
    do {                        \
        str_traceInt64(NUMBER); \
    } while (0)
#else
#define TRACE_INT64(NUMBER)
#endif  // DEBUG

bool str_isPrintableAsciiWithoutSpaces(const uint8_t* buffer, size_t bufferSize);
bool str_isPrintableAsciiWithSpaces(const uint8_t* buffer, size_t bufferSize);

bool str_isUnambiguousAscii(const uint8_t* buffer, size_t bufferSize);
