#pragma once

#include <stdio.h>
#include <stdlib.h>

// Mock LEDGER_ASSERT for unit tests - doesn't depend on C standard assert()
#ifndef LEDGER_ASSERT
#define LEDGER_ASSERT(test, format, ...)                                                        \
    do {                                                                                        \
        if (!(test)) {                                                                          \
            fprintf(stderr, "LEDGER_ASSERT FAILED (%s:%d): " format "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
            abort();                                                                            \
        }                                                                                       \
    } while (0)
#endif
