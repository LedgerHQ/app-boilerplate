#include <stdint.h>
#include <stdio.h>

int io_send_sw(uint16_t sw) {
    fprintf(stderr, "[mock io_send_sw] sw=0x%04x\n", sw);
    return 0;
}

uintptr_t pic(uintptr_t linked_address) {
    return linked_address;
}
