#pragma once

// Mock exceptions.h for unit tests
// THROW is used for error handling in device code
// In tests, we'll use assert(0) to signal errors

#include <assert.h>
#include <stdio.h>

// Mock THROW - in tests, this should not be reached for valid inputs
// If it is reached, the test will fail with a clear message
#define THROW(x) do { \
    fprintf(stderr, "THROW(%d) called at %s:%d\n", (int)(x), __FILE__, __LINE__); \
    assert(0 && "THROW should not be reached in valid tests"); \
} while(0)

// Mock TRY/CATCH for tests that need it
#define TRY
#define CATCH(x) if (0)
#define FINALLY
