#pragma once

#include "types.h"

/**
 * Structure for public key context.
 */
typedef struct {
    cx_ecfp_public_key_t public_key;
    uint8_t chain_code[32];
    uint32_t bip32_path[MAX_BIP32_PATH];
    size_t bip32_path_len;
} pubkey_ctx_t;

/**
 * Public key context.
 */
extern pubkey_ctx_t pk_ctx;

void context_reset_pubkey(void);
