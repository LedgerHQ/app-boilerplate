#pragma once

// Mock io.h for unit tests
// Provides stubs for IO functions not needed in unit tests

#include <stdint.h>
#include <stddef.h>

// Mock IO send function declarations
int io_send_sw(uint16_t sw);

int io_send_response_pointer(const uint8_t *buffer, size_t bufferLength, uint16_t sw);
