#include "cbor.h"
#include "sw.h"
#include "read.h"
#include "write.h"

// Note(ppershing): consume functions should either
// a) *consume* expected value, or
// b) *throw* but not consume anything from the stream

static const uint64_t VALUE_W1_UPPER_THRESHOLD = 24;
static const uint64_t VALUE_W2_UPPER_THRESHOLD = (uint64_t) 1 << 8;
static const uint64_t VALUE_W4_UPPER_THRESHOLD = (uint64_t) 1 << 16;
static const uint64_t VALUE_W8_UPPER_THRESHOLD = (uint64_t) 1 << 32;

bool cbor_parseToken(const uint8_t* buf, size_t size, cbor_token_t* out_token) {
    LEDGER_ASSERT(buf != NULL, "NULL buf");
    LEDGER_ASSERT(out_token != NULL, "NULL out_token");

    // Need at least 1 byte for the tag
    if (size < 1) {
        return false;
    }

    const uint8_t tag = buf[0];
    cbor_token_t result;

    // tag extensions first
    if (tag == CBOR_TYPE_ARRAY_INDEF || tag == CBOR_TYPE_INDEF_END) {
        result.type = tag;
        result.width = 0;
        result.value = 0;
        *out_token = result;
        return true;
    }

    result.type = tag & CBOR_TYPE_MASK;

    switch (result.type) {
        case CBOR_TYPE_UNSIGNED:
        case CBOR_TYPE_NEGATIVE:
        case CBOR_TYPE_BYTES:
        case CBOR_TYPE_ARRAY:
        case CBOR_TYPE_MAP:
        case CBOR_TYPE_TAG:
            break;
        default:
            // We don't know how to parse others
            // (particularly CBOR_TYPE_PRIMITIVES)
            return false;
    }

    const uint8_t val = (tag & CBOR_VALUE_MASK);
    if (val < 24) {
        result.width = 0;
        result.value = val;
    } else {
        // Holds minimum value for a given byte-width.
        // Anything below this is not canonical CBOR as
        // it could be represented by a shorter CBOR notation
        uint64_t limit_min;
        size_t required_size;

        switch (val) {
            case 24:
                required_size = 1 + 1;
                if (size < required_size) {
                    return false;
                }
                result.width = 1;
                result.value = (buf + 1)[0];
                limit_min = VALUE_W1_UPPER_THRESHOLD;
                break;
            case 25:
                required_size = 1 + 2;
                if (size < required_size) {
                    return false;
                }
                result.width = 2;
                result.value = read_u16_be(buf + 1, 0);
                limit_min = VALUE_W2_UPPER_THRESHOLD;
                break;
            case 26:
                required_size = 1 + 4;
                if (size < required_size) {
                    return false;
                }
                result.width = 4;
                result.value = read_u32_be(buf + 1, 0);
                limit_min = VALUE_W4_UPPER_THRESHOLD;
                break;
            case 27:
                required_size = 1 + 8;
                if (size < required_size) {
                    return false;
                }
                result.width = 8;
                result.value = read_u64_be(buf + 1, 0);
                limit_min = VALUE_W8_UPPER_THRESHOLD;
                break;
            default:
                // Values above 27 are not valid in CBOR.
                // Exception is indefinite length marker
                // but this has been handled separately.
                return false;
        }

        if (result.value < limit_min) {
            // This isn't canonical CBOR
            return false;
        }
    }

    if (result.type == CBOR_TYPE_NEGATIVE) {
        if (result.value > INT64_MAX) {
            return false;
        }
        int64_t negativeValue;
        if (result.value < INT64_MAX) {
            negativeValue = -((int64_t)(result.value + 1));
        } else {
            negativeValue = INT64_MIN;
        }
        result.value = negativeValue;
    }

    *out_token = result;
    return true;
}

size_t cbor_writeToken(uint8_t type, uint64_t value, uint8_t* buffer, size_t bufferSize) {
    ASSERT(bufferSize < BUFFER_SIZE_PARANOIA);

#define CHECK_BUF_LEN(requiredSize) \
    if ((size_t) requiredSize > bufferSize) THROW(ERR_DATA_TOO_LARGE);
    if (type == CBOR_TYPE_ARRAY_INDEF || type == CBOR_TYPE_INDEF_END || type == CBOR_TYPE_NULL) {
        CHECK_BUF_LEN(1);
        buffer[0] = type;
        return 1;
    }

    if (type & CBOR_VALUE_MASK) {
        // type should not have any value
        THROW(ERR_UNEXPECTED_TOKEN);
    }

    // Check sanity
    switch (type) {
        case CBOR_TYPE_NEGATIVE: {
            int64_t negativeValue;
            // reinterpret an actually negative value hidden in an unsigned in the safe way
            STATIC_ASSERT(SIZEOF(negativeValue) == SIZEOF(value),
                          "incompatible signed and unsigned type sizes");
            memmove(&negativeValue, &value, SIZEOF(value));
            if (negativeValue >= 0) {
                THROW(ERR_UNEXPECTED_TOKEN);
            }
            value = (uint64_t)(-negativeValue) - 1;
        }
            __attribute__((fallthrough));
        case CBOR_TYPE_UNSIGNED:
        case CBOR_TYPE_BYTES:
        case CBOR_TYPE_TEXT:
        case CBOR_TYPE_ARRAY:
        case CBOR_TYPE_MAP:
        case CBOR_TYPE_TAG:
            break;
        default:
            // not supported
            THROW(ERR_UNEXPECTED_TOKEN);
    }

    // Warning(ppershing): It might be tempting but we don't want to call stream_appendData() twice
    // Instead we have to construct the whole buffer at once to make append operation atomic.
    #define u1be_write(buffer, value) (buffer)[0] = (value);

    if (value < VALUE_W1_UPPER_THRESHOLD) {
        CHECK_BUF_LEN(1);
        u1be_write(buffer, (uint8_t)(type | value));
        return 1;
    } else if (value < VALUE_W2_UPPER_THRESHOLD) {
        CHECK_BUF_LEN(1 + 1);
        u1be_write(buffer, type | 24);
        u1be_write(buffer + 1, (uint8_t) value);
        return 1 + 1;
    } else if (value < VALUE_W4_UPPER_THRESHOLD) {
        CHECK_BUF_LEN(1 + 2);
        u1be_write(buffer, type | 25);
        write_u16_be(buffer + 1, 0, (uint16_t) value);
        return 1 + 2;
    } else if (value < VALUE_W8_UPPER_THRESHOLD) {
        CHECK_BUF_LEN(1 + 4);
        u1be_write(buffer, type | 26);
        write_u32_be(buffer + 1, 0, (uint32_t) value);
        return 1 + 4;
    } else {
        CHECK_BUF_LEN(1 + 8);
        u1be_write(buffer, type | 27);
        write_u64_be(buffer + 1, 0, value);
        return 1 + 8;
    }
    #undef u1be_write
#undef CHECK_BUF_LEN
}

bool cbor_mapKeyFulfillsCanonicalOrdering(const uint8_t* previousBuffer,
                                          size_t previousSize,
                                          const uint8_t* nextBuffer,
                                          size_t nextSize) {
    ASSERT(previousSize < BUFFER_SIZE_PARANOIA);
    ASSERT(nextSize < BUFFER_SIZE_PARANOIA);

    if (previousSize != nextSize) {
        return previousSize < nextSize;
    }
    for (size_t i = 0; i < previousSize; ++i) {
        if (*previousBuffer != *nextBuffer) {
            return *previousBuffer < *nextBuffer;
        }
        ++previousBuffer;
        ++nextBuffer;
    }
    // key duplication is an error
    return false;
}
