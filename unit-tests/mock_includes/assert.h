#pragma once

// Provide the project's helper macros first.
#include "../../src/utils/assert.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef assert
#define assert(expr)                                                                      \
    do {                                                                                  \
        if (!(expr)) {                                                                    \
            fprintf(stderr, "assertion failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
            abort();                                                                      \
        }                                                                                 \
    } while (0)
#endif
