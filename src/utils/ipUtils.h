#pragma once

#include "os.h"

#define MAX_IPV4_STR_LENGTH (sizeof "255.255.255.255")
#define MAX_IPV6_STR_LENGTH (sizeof "ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255")

void inet_ntop4(const uint8_t* src, char* dst, size_t dstSize);
void inet_ntop6(const uint8_t* src, char* dst, size_t dstSize);
