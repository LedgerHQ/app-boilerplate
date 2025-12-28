#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#ifdef HAVE_PRINTF
typedef enum {
    LENGTH_NONE,
    LENGTH_HH,
    LENGTH_H,
    LENGTH_L,
    LENGTH_LL,
    LENGTH_Z,
    LENGTH_T,
    LENGTH_J,
    LENGTH_CAPL,
} length_modifier_t;

static length_modifier_t
find_length_modifier(const char* start, const char* end) {
    length_modifier_t modifier = LENGTH_NONE;
    for (const char* it = start; it < end; ++it) {
        if (it + 1 < end && it[0] == 'l' && it[1] == 'l') {
            modifier = LENGTH_LL;
            ++it;
            continue;
        }
        if (it + 1 < end && it[0] == 'h' && it[1] == 'h') {
            modifier = LENGTH_HH;
            ++it;
            continue;
        }
        switch (it[0]) {
            case 'l':
                modifier = LENGTH_L;
                break;
            case 'h':
                modifier = LENGTH_H;
                break;
            case 'z':
                modifier = LENGTH_Z;
                break;
            case 't':
                modifier = LENGTH_T;
                break;
            case 'j':
                modifier = LENGTH_J;
                break;
            case 'L':
                modifier = LENGTH_CAPL;
                break;
            default:
                break;
        }
    }
    return modifier;
}

static void
print_hex(FILE* stream, const uint8_t* buffer, size_t length) {
    static const char HEX_DIGITS[] = "0123456789abcdef";
    for (size_t i = 0; i < length; ++i) {
        fputc(HEX_DIGITS[buffer[i] >> 4], stream);
        fputc(HEX_DIGITS[buffer[i] & 0x0f], stream);
    }
}

void PRINTF(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    const char* ptr = fmt;
    while (*ptr != '\0') {
        const char* percent = strchr(ptr, '%');
        if (percent == NULL) {
            fputs(ptr, stderr);
            break;
        }

        if (percent > ptr) {
            fwrite(ptr, 1, percent - ptr, stderr);
        }

        ptr = percent;
        if (ptr[1] == '%') {
            fputc('%', stderr);
            ptr += 2;
            continue;
        }

        if (ptr[1] == '.' && ptr[2] == '*' && (ptr[3] == 'h' || ptr[3] == 'H')) {
            int len = va_arg(ap, int);
            const uint8_t* buffer = va_arg(ap, const uint8_t*);
            if (len > 0 && buffer != NULL) {
                print_hex(stderr, buffer, (size_t) len);
            }
            ptr += 4;
            continue;
        }

        const char* spec_end = ptr + 1;
        while (*spec_end != '\0' &&
               !isalpha((unsigned char)*spec_end) &&
               *spec_end != '%') {
            ++spec_end;
        }
        if (*spec_end == '\0') {
            break;
        }
        ++spec_end;

        size_t spec_length = spec_end - ptr;
        char spec_fmt[64];
        if (spec_length >= sizeof(spec_fmt)) {
            spec_length = sizeof(spec_fmt) - 1;
        }
        memcpy(spec_fmt, ptr, spec_length);
        spec_fmt[spec_length] = '\0';
        char conversion = spec_fmt[spec_length - 1];
        length_modifier_t modifier = find_length_modifier(ptr + 1, ptr + spec_length - 1);

        switch (conversion) {
            case 'd':
            case 'i': {
                switch (modifier) {
                    case LENGTH_LL: {
                        long long value = va_arg(ap, long long);
                        fprintf(stderr, spec_fmt, value);
                        break;
                    }
                    case LENGTH_L: {
                        long value = va_arg(ap, long);
                        fprintf(stderr, spec_fmt, value);
                        break;
                    }
                    case LENGTH_H:
                    case LENGTH_HH:
                    case LENGTH_NONE:
                    default: {
                        int value = va_arg(ap, int);
                        fprintf(stderr, spec_fmt, value);
                        break;
                    }
                }
                break;
            }

            case 'u':
            case 'o':
            case 'x':
            case 'X': {
                switch (modifier) {
                    case LENGTH_LL: {
                        unsigned long long value = va_arg(ap, unsigned long long);
                        fprintf(stderr, spec_fmt, value);
                        break;
                    }
                    case LENGTH_L: {
                        unsigned long value = va_arg(ap, unsigned long);
                        fprintf(stderr, spec_fmt, value);
                        break;
                    }
                    case LENGTH_Z: {
                        size_t value = va_arg(ap, size_t);
                        fprintf(stderr, spec_fmt, value);
                        break;
                    }
                    case LENGTH_T: {
                        ptrdiff_t value = va_arg(ap, ptrdiff_t);
                        fprintf(stderr, spec_fmt, value);
                        break;
                    }
                    case LENGTH_J: {
                        uintmax_t value = va_arg(ap, uintmax_t);
                        fprintf(stderr, spec_fmt, value);
                        break;
                    }
                    case LENGTH_H:
                    case LENGTH_HH:
                    case LENGTH_NONE:
                    default: {
                        unsigned int value = va_arg(ap, unsigned int);
                        fprintf(stderr, spec_fmt, value);
                        break;
                    }
                }
                break;
            }

            case 'p': {
                void* value = va_arg(ap, void*);
                fprintf(stderr, spec_fmt, value);
                break;
            }

            case 'c': {
                int value = va_arg(ap, int);
                fprintf(stderr, spec_fmt, value);
                break;
            }

            case 's': {
                const char* value = va_arg(ap, const char*);
                fprintf(stderr, spec_fmt, value);
                break;
            }

            case 'f':
            case 'F':
            case 'g':
            case 'G':
            case 'e':
            case 'E':
            case 'a':
            case 'A': {
                if (modifier == LENGTH_CAPL) {
                    long double value = va_arg(ap, long double);
                    fprintf(stderr, spec_fmt, value);
                } else {
                    double value = va_arg(ap, double);
                    fprintf(stderr, spec_fmt, value);
                }
                break;
            }

            default:
                fprintf(stderr, "%s", spec_fmt);
                break;
        }

        ptr = spec_end;
    }

    va_end(ap);
}
#endif
