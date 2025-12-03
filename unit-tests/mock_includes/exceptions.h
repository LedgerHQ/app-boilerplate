#pragma once

// Mock exceptions.h for unit tests.
// The production SDK header used to expose THROW/TRY/CATCH, but the new
// codebase must not depend on that mechanism anymore. The mock os.h
// redefines those macros to static assertions so any accidental use fails
// at compile time. This stub header simply provides compatibility for
// modules that still include exceptions.h without needing anything from it.
