#include <stdarg.h>
#include <stdio.h>

#ifdef HAVE_PRINTF
void PRINTF(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}
#endif
