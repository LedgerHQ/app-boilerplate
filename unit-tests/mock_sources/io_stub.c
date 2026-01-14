#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

__attribute__((weak)) int io_send_sw(uint16_t swo) {
    fprintf(stderr, "[mock io_send_sw] sw=0x%04x\n", swo);
    return 0;
}

// Weak symbol - will be overridden by real implementation if app_context.c is linked
__attribute__((weak)) void send_swo_and_reset(uint16_t swo) {
    fprintf(stderr, "[mock send_swo_and_reset] sw=0x%04x\n", swo);
    io_send_sw(swo);
}

bool buffer_can_read(const void *buffer, size_t n) {
    // Mock implementation - check if buffer has at least n bytes available
    typedef struct {
        const uint8_t *ptr;
        size_t size;
        size_t offset;
    } buffer_t;

    const buffer_t *buf = (const buffer_t *)buffer;
    return (buf->size - buf->offset) >= n;
}

uintptr_t pic(uintptr_t linked_address) {
    return linked_address;
}
