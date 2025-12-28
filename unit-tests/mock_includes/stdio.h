#pragma once

#include <stdarg.h>
#include <stddef.h>

/* Mock stdio.h for unit tests that provides PRINTF function.
   We provide minimal declarations needed for fprintf and vfprintf to work. */

/* Standard FILE type from glibc */
typedef struct {
    int dummy;
} FILE;

extern FILE *stderr;

/* Standard C library I/O functions */
int fprintf(FILE *stream, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vfprintf(FILE *stream, const char *format, va_list ap);
int fputc(int c, FILE *stream);
int fputs(const char *s, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);

#ifdef HAVE_PRINTF
void PRINTF(const char *fmt, ...);
#else
#define PRINTF(...)
#endif
