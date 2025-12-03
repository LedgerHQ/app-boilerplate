#pragma once

#include "../src/types.h"

typedef struct {
    uint8_t cla;
    uint8_t ins;
    uint8_t p1;
    uint8_t p2;
    size_t lc;
    const uint8_t *data;
} command_t;
