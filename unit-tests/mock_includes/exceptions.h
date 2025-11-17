#pragma once

// Mock exceptions.h for unit tests
// When os.h is included (which defines THROW, TRY, CATCH, FINALLY),
// we don't need to redefine them. This header is kept for compatibility
// with code that includes exceptions.h directly.
