
#pragma once

#include <stdint.h>  // uint*_t

#include "os.h"

int crypto_derive_private_key(cx_ecfp_private_key_t *private_key,
                              uint8_t chain_code[static 32],
                              uint32_t *bip32_path,
                              uint8_t bip32_path_len);

int crypto_init_public_key(cx_ecfp_private_key_t *private_key,
                           cx_ecfp_public_key_t *public_key,
                           uint8_t raw_public_key[static 64]);

int crypto_sign_message(void);
