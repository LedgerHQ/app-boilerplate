#pragma once

#include <stdint.h>   // uint*_t
#include <stddef.h>   // size_t
#include <stdbool.h>  // bool

bool address_from_pubkey(uint8_t public_key[static 64], uint8_t *out, size_t out_len);
