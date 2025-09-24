#pragma once

#include "os.h"

#define IPV4_STR_SIZE_MAX (sizeof "255.255.255.255")
#define IPV6_STR_SIZE_MAX (sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")

void inet_ntop4(const uint8_t* src, char* dst, size_t dstSize);
void inet_ntop6(const uint8_t* src, char* dst, size_t dstSize);
