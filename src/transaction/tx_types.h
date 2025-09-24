#pragma once

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t

#define MAX_TX_LEN   510
#define ADDRESS_LEN  20
#define MAX_MEMO_LEN 465  // 510 - ADDRESS_LEN - 2*SIZE(U64) - SIZE(MAX_VARINT)

typedef struct {
    uint64_t nonce;     /// nonce (8 bytes)
    uint64_t value;     /// amount value (8 bytes)
    uint64_t fee;       /// fee (8 bytes)
    uint8_t *to;        /// pointer to address (20 bytes)
    uint8_t *memo;      /// memo (variable length)
    uint64_t memo_len;  /// length of memo (8 bytes)
} transaction_t;
