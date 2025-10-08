#pragma once

#include "os.h"

#include "assert.h"

// Does not compile if x is pointer of some kind
// See http://zubplot.blogspot.com/2015/01/gcc-is-wonderful-better-arraysize-macro.html
#define ARRAY_NOT_A_PTR(x)                                                                 \
    (sizeof(__typeof__(                                                                    \
         int[1 - 2 * !!__builtin_types_compatible_p(__typeof__(x), __typeof__(&x[0]))])) * \
     0)

// Safe array length, does not compile if you accidentally supply a pointer
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]) + ARRAY_NOT_A_PTR(arr))

#ifndef FUZZING
// Does not compile if x *might* be a pointer of some kind
// Might produce false positives on small structs...
// Note: ARRAY_NOT_A_PTR does not compile if arg is a struct so this is a workaround
#define SIZEOF_NOT_A_PTR(var) (sizeof(__typeof(int[0 - (sizeof(var) == sizeof((void *) 0))])) * 0)

// Safe version of SIZEOF, does not compile if you accidentally supply a pointer
#define SIZEOF(var) (sizeof(var) + SIZEOF_NOT_A_PTR(var))

#else
#define SIZEOF(var) sizeof(var)
#endif

// Helper function to check APDU request parameters
#define VALIDATE(cond, error)                                           \
    do {                                                                \
        if (!(cond)) {                                                  \
            PRINTF("Validation Error in %s: %d\n", __FILE__, __LINE__); \
            THROW(error);                                               \
        }                                                               \
    } while (0)

// Any buffer claiming to be longer than this is a bug
// (we anyway have only 4KB of memory)
#define BUFFER_SIZE_PARANOIA 1024

#define PTR_PIC(ptr) ((__typeof__(ptr)) PIC(ptr))

#define ITERATE(it, arr) for (__typeof__(&(arr[0])) it = BEGIN(arr); it < END(arr); it++)

// Note: unused removes unused warning but does not warn if you suddenly
// start using such variable. deprecated deals with that.
#define MARK_UNUSED __attribute__((unused, deprecated))

// Note: inlining can increase stack memory usage
// where we really do not want it
#define __noinline_due_to_stack__ __attribute__((noinline))

#ifdef HAVE_PRINTF
#define TRACE(...)                              \
    do {                                        \
        PRINTF("[%s:%d] ", __func__, __LINE__); \
        PRINTF("" __VA_ARGS__);                 \
        PRINTF("\n");                           \
    } while (0)
#else
#define TRACE(...)
#endif

#ifdef HAVE_PRINTF
#define TRACE_BUFFER(BUF, SIZE) TRACE("%.*h", SIZE, BUF);
#else
#define TRACE_BUFFER(BUF, SIZE)
#endif

#define IS_SIGNED_TYPE(type) (((type)(-1)) < 0)
#define IS_SIGNED(var)       (((typeof(var))(-1)) < 0)

/**
 * Parse item inclusion flag for optional transaction fields.
 *
 * @param value The inclusion flag byte (ITEM_INCLUDED_YES or ITEM_INCLUDED_NO)
 * @param[out] result Pointer to store the result (true if included, false otherwise)
 * @return true if parsing succeeded, false if value is invalid
 */
#include "constants.h"

static inline bool parseIncluded(uint8_t value, bool* result) {
    switch (value) {
        case ITEM_INCLUDED_YES:
            *result = true;
            return true;
        case ITEM_INCLUDED_NO:
            *result = false;
            return true;
        default:
            return false;
    }
}
