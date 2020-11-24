#pragma once

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t

#define MAX_TX_LEN   510
#define ADDRESS_LEN  20
#define MAX_MEMO_LEN 465  // 510 - ADDRESS_LEN - 2*SIZE(U64) - SIZE(MAX_VARINT)

typedef enum {
    PARSING_OK = 1,
    NONCE_PARSING_ERROR = -1,
    TO_PARSING_ERROR = -2,
    VALUE_PARSING_ERROR = -3,
    MEMO_LENGTH_ERROR = -4,
    MEMO_PARSING_ERROR = -5,
    MEMO_ENCODING_ERROR = -6,
    WRONG_LENGTH_ERROR = -7
} parser_status_e;

typedef struct {
    uint64_t nonce;     /// 8 bytes nonce
    uint64_t value;     /// 8 bytes amount value
    uint8_t *to;        /// pointer to 20 bytes address
    uint8_t *memo;      /// variable length memo (ascii encoded)
    uint64_t memo_len;  /// 8 bytes length of memo (varint 1-8 bytes)
} transaction_t;
