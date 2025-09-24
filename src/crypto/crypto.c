/*****************************************************************************
 *   Ledger SDK.
 *   (c) 2023 Ledger SAS.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *****************************************************************************/
#include <stdint.h>   // uint*_t
#include <string.h>   // explicit_bzero
#include <stdbool.h>  // bool

#include "cx.h"
#include "os.h"

static cx_err_t crypto_init_privkey(const uint32_t* path,
                                    size_t path_len,
                                    cx_ecfp_256_extended_private_key_t* privkey,
                                    uint8_t* chain_code) {
    cx_err_t error = CX_OK;
    uint8_t raw_privkey[64];

    // Derive private key according to BIP32 path
    CX_CHECK(os_derive_bip32_no_throw(CX_CURVE_Ed25519, path, path_len, raw_privkey, chain_code));

    // Init privkey from raw
    // Do not use cx_ecfp_init_private_key_no_throw as it doesn't
    // support 64 bytes for CX_CURVE_Ed25519 curve
    privkey->curve = CX_CURVE_Ed25519;
    privkey->d_len = sizeof(privkey->d);
    memmove(privkey->d, raw_privkey, sizeof(privkey->d));

end:
    explicit_bzero(raw_privkey, sizeof(raw_privkey));

    // CX_CHECK above would set the value of `error` in case of error
    if (error != CX_OK) {
        // Make sure the caller doesn't use uninitialized data in case
        // the return code is not checked.
        explicit_bzero(privkey, sizeof(cx_ecfp_256_extended_private_key_t));
    }
    return error;
}

WARN_UNUSED_RESULT cx_err_t crypto_get_pubkey(const uint32_t* path,
                                              size_t path_len,
                                              uint8_t raw_pubkey[static 65],
                                              uint8_t* chain_code) {
    cx_err_t error = CX_OK;

    cx_ecfp_256_extended_private_key_t privkey;
    cx_ecfp_256_public_key_t pubkey;

    // Derive private key according to BIP32 path
    CX_CHECK(crypto_init_privkey(path, path_len, &privkey, chain_code));

    // Generate associated pubkey
    // Do not use cx_ecfp_generate_pair2_no_throw as it doesn't
    // support 64 bytes for CX_CURVE_Ed25519 curve
    CX_CHECK(cx_eddsa_get_public_key_no_throw((const struct cx_ecfp_256_private_key_s*) &privkey,
                                              CX_SHA512,
                                              &pubkey,
                                              NULL,
                                              0,
                                              NULL,
                                              0));

    // Check pubkey length then copy it to raw_pubkey
    if (pubkey.W_len != 65) {
        error = CX_EC_INVALID_CURVE;
        goto end;
    }
    memmove(raw_pubkey, pubkey.W, pubkey.W_len);

end:
    explicit_bzero(&privkey, sizeof(privkey));

    // CX_CHECK above would set the value of `error` in case of error
    if (error != CX_OK) {
        // Make sure the caller doesn't use uninitialized data in case
        // the return code is not checked.
        explicit_bzero(raw_pubkey, 65);
    }
    return error;
}

WARN_UNUSED_RESULT cx_err_t crypto_eddsa_sign(const uint32_t* path,
                                              size_t path_len,
                                              const uint8_t* hash,
                                              size_t hash_len,
                                              uint8_t* sig,
                                              size_t* sig_len) {
    cx_err_t error = CX_OK;
    cx_ecfp_256_extended_private_key_t privkey;
    size_t size;
    size_t buf_len = *sig_len;

    if (sig_len == NULL) {
        error = CX_INVALID_PARAMETER_VALUE;
        goto end;
    }
    // Derive private key according to BIP32 path
    CX_CHECK(crypto_init_privkey(path, path_len, &privkey, NULL));

    CX_CHECK(cx_eddsa_sign_no_throw((const struct cx_ecfp_256_private_key_s*) &privkey,
                                    CX_SHA512,
                                    hash,
                                    hash_len,
                                    sig,
                                    *sig_len));

    CX_CHECK(cx_ecdomain_parameters_length(CX_CURVE_Ed25519, &size));
    *sig_len = size * 2;

end:
    explicit_bzero(&privkey, sizeof(privkey));

    // CX_CHECK above would set the value of `error` in case of error
    if (error != CX_OK) {
        // Make sure the caller doesn't use uninitialized data in case
        // the return code is not checked.
        explicit_bzero(sig, buf_len);
    }
    return error;
}
