#include <stddef.h>
#include <stdint.h>

int bytes_to_lowercase_hex(char* out, size_t outl, const void* value, size_t len) {
    static const char hex_table[] = "0123456789abcdef";
    const uint8_t* bytes = (const uint8_t*) value;
    if (outl < (len * 2) + 1) {
        return -1;
    }
    for (size_t i = 0; i < len; ++i) {
        out[2 * i] = hex_table[(bytes[i] >> 4) & 0xF];
        out[2 * i + 1] = hex_table[bytes[i] & 0xF];
    }
    out[len * 2] = '\0';
    return 0;
}
