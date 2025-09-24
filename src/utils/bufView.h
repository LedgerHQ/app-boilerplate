#pragma once

// TODO this is legacy code that should eventually be removed; using buffer_t would be consistent with other ledger apps
// e.g. bitcoin app uses buffer_write_u8, and there is buffer_read_u64 etc. which we already use
// buffer_write is not part of standard app library now, but we might suggest it, or just copy bitcoin code

#include "../globals.h"
#include "cbor.h"
#include "assert.h"
#include "utils.h"
#include "sw.h"
#include "read.h"

typedef const uint8_t read_view__base_type;
typedef uint8_t write_view__base_type;

#define __DEFINE_VIEW(name)                                                                \
    typedef struct {                                                                       \
        name##__base_type* begin;                                                          \
        name##__base_type* ptr;                                                            \
        name##__base_type* end;                                                            \
    } name##_t;                                                                            \
                                                                                           \
    static inline name##_t make_##name(name##__base_type* begin, name##__base_type* end) { \
        name##_t result = {.begin = begin, .ptr = begin, .end = end};                      \
        return result;                                                                     \
    }                                                                                      \
                                                                                           \
    static inline void name##_check(const name##_t* view) {                                \
        ASSERT(view->begin <= view->ptr);                                                  \
        ASSERT(view->ptr <= view->end);                                                    \
        ASSERT(view->end - view->begin <= BUFFER_SIZE_PARANOIA);                           \
    }                                                                                      \
                                                                                           \
    static inline size_t name##_remaining_size(const name##_t* view) {                     \
        name##_check(view);                                                                \
        STATIC_ASSERT(sizeof(view->end - view->ptr) == sizeof(size_t), "bad size");        \
        return (size_t)(view->end - view->ptr);                                            \
    }                                                                                      \
                                                                                           \
    static inline size_t name##_processed_size(const name##_t* view) {                     \
        name##_check(view);                                                                \
        STATIC_ASSERT(sizeof(view->end - view->ptr) == sizeof(size_t), "bad size");        \
        return (size_t)(view->ptr - view->begin);                                          \
    }

__DEFINE_VIEW(read_view)
__DEFINE_VIEW(write_view)

#define __DEFINE_VIEW_skip(name, err)                              \
    static inline void name##_skip(name##_t* view, size_t bytes) { \
        name##_check(view);                                        \
        VALIDATE(bytes <= name##_remaining_size(view), err);       \
        (view)->ptr += bytes;                                      \
        name##_check(view);                                        \
    }

__DEFINE_VIEW_skip(read_view, ERR_NOT_ENOUGH_INPUT)
    __DEFINE_VIEW_skip(write_view, ERR_DATA_TOO_LARGE)
#define __VIEW_GENERIC_TEMPLATE(expr, suffix) \
    _Generic((expr), read_view_t * : read_view_##suffix, write_view_t * : write_view_##suffix)

#define view_remainingSize(view)   __VIEW_GENERIC_TEMPLATE(view, remaining_size)(view)
#define view_processedSize(view)   __VIEW_GENERIC_TEMPLATE(view, processed_size)(view)
#define view_skipBytes(view, size) __VIEW_GENERIC_TEMPLATE(view, skip)(view, size)
#define view_check(view)           __VIEW_GENERIC_TEMPLATE(view, check)(view)

        static inline void view_appendToken(write_view_t* view, uint8_t type, uint64_t value) {
    ASSERT(view_remainingSize(view) <= BUFFER_SIZE_PARANOIA);

    view->ptr += cbor_writeToken(type, value, view->ptr, view_remainingSize(view));
}

static inline void view_appendBuffer(write_view_t* view,
                                     const uint8_t* sourceBuffer,
                                     size_t length) {
    view_check(view);
    VALIDATE(length <= view_remainingSize(view), ERR_DATA_TOO_LARGE);
    memmove(view->ptr, sourceBuffer, length);
    view->ptr += length;
    view_check(view);
}

static inline cbor_token_t view_parseToken(read_view_t* view) {
    const cbor_token_t token = cbor_parseToken(view->ptr, view_remainingSize(view));
    view_skipBytes(view, token.width + 1);
    return token;
}

// copies <length> bytes from the view to the buffer
// throws ERR_INVALID_DATA if not enough data
static inline void view_parseBuffer(uint8_t* destBuffer, read_view_t* view, size_t length) {
    ASSERT(length < BUFFER_SIZE_PARANOIA);

    VALIDATE(view_remainingSize(view) >= length, ERR_INVALID_DATA);
    memmove(destBuffer, view->ptr, length);
    view_skipBytes(view, length);
}

// Note(ppershing): these macros expand to two arguments!
#define VIEW_REMAINING_TO_TUPLE_BUF_SIZE(view) (view)->ptr, view_remainingSize(view)
#define VIEW_PROCESSED_TO_TUPLE_BUF_SIZE(view) (view)->begin, view_processedSize(view)

static inline uint8_t parse_u1be(read_view_t* view) {
    VALIDATE(view_remainingSize(view) >= 1, ERR_INVALID_DATA);
    uint8_t result = view->ptr[0];
    view->ptr += 1;
    return result;
};

static inline uint16_t parse_u2be(read_view_t* view) {
    VALIDATE(view_remainingSize(view) >= 2, ERR_INVALID_DATA);
    uint16_t result = read_u16_be(view->ptr, 0);
    view->ptr += 2;
    return result;
};

static inline uint32_t parse_u4be(read_view_t* view) {
    VALIDATE(view_remainingSize(view) >= 4, ERR_INVALID_DATA);
    uint32_t result = read_u32_be(view->ptr, 0);
    view->ptr += 4;
    return result;
};

static inline uint64_t parse_u8be(read_view_t* view) {
    VALIDATE(view_remainingSize(view) >= 8, ERR_INVALID_DATA);
    uint64_t result = read_u64_be(view->ptr, 0);
    view->ptr += 8;
    return result;
};

static inline int64_t parse_int64be(read_view_t* view) {
    // works with "Int64BE(value, 10).toBuffer()" which we use to serialize int64
    return (int64_t) parse_u8be(view);
};

static inline bool parse_bool(read_view_t* view) {
    uint8_t value = parse_u1be(view);

    switch (value) {
        case 0:
            return false;
        case 1:
            return true;
        default:
            THROW(ERR_INVALID_DATA);
    }
}
