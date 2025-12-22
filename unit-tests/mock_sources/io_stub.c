#include <stdint.h>

int io_send_sw(uint16_t sw) {
    (void) sw;
    return 0;
}

uintptr_t pic(uintptr_t linked_address) {
    return linked_address;
}
