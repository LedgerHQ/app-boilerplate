#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cx.h"

#include "decorators.h"
#include "crypto.h"
#include "constants.h"
#include "keyDerivation/keyDerivation.h"
#include "crypto_mock_data.h"

#define RAW_PUBKEY_SIZE 65

static bool path_matches(const uint32_t* lhs, size_t lhs_len, const uint32_t* rhs, size_t rhs_len) {
    if (lhs_len != rhs_len) {
        return false;
    }
    for (size_t i = 0; i < lhs_len; i++) {
        if (lhs[i] != rhs[i]) {
            return false;
        }
    }
    return true;
}

static const mock_path_data_t* find_path_entry(const uint32_t* path, size_t path_len) {
    for (size_t i = 0; i < MOCK_PATH_COUNT; i++) {
        if (path_matches(path, path_len, MOCK_PATHS[i].path, MOCK_PATHS[i].path_len)) {
            return &MOCK_PATHS[i];
        }
    }
    return NULL;
}

static const mock_signature_data_t* find_signature_entry(const uint32_t* path,
                                                         size_t path_len,
                                                         const uint8_t* message,
                                                         size_t message_len) {
    for (size_t i = 0; i < MOCK_SIGNATURE_COUNT; i++) {
        const mock_signature_data_t* entry = &MOCK_SIGNATURES[i];
        if (!path_matches(path, path_len, entry->path, entry->path_len)) {
            continue;
        }
        if (entry->message_len != message_len) {
            continue;
        }
        if (memcmp(entry->message, message, message_len) != 0) {
            continue;
        }
        return entry;
    }
    return NULL;
}

static void encode_raw_pubkey(const uint8_t public_key[32], uint8_t raw_pubkey[RAW_PUBKEY_SIZE]) {
    memset(raw_pubkey, 0, RAW_PUBKEY_SIZE);
    raw_pubkey[0] = 0x04;

    uint8_t y_le[32];
    memcpy(y_le, public_key, 32);
    uint8_t sign_bit = (uint8_t)(y_le[31] & 0x80u);
    y_le[31] &= 0x7Fu;

    for (size_t i = 0; i < 32; i++) {
        raw_pubkey[RAW_PUBKEY_SIZE - 1 - i] = y_le[i];
    }
    raw_pubkey[32] = (uint8_t)(sign_bit ? 0x01 : 0x00);
}

cx_err_t crypto_get_pubkey(const uint32_t* path,
                           size_t path_len,
                           uint8_t raw_pubkey[static RAW_PUBKEY_SIZE],
                           uint8_t* chain_code) {
    const mock_path_data_t* entry = find_path_entry(path, path_len);
    if (entry == NULL) {
        fprintf(stderr, "crypto_mock: missing pubkey path len=%zu [", path_len);
        for (size_t i = 0; i < path_len; i++) {
            fprintf(stderr, "%s0x%08x", (i == 0 ? "" : ", "), path[i]);
        }
        fprintf(stderr, "]\n");
        return CX_INTERNAL_ERROR;
    }
    encode_raw_pubkey(entry->public_key, raw_pubkey);
    memcpy(chain_code, entry->chain_code, CHAIN_CODE_SIZE);
    return CX_OK;
}

cx_err_t crypto_eddsa_sign(const uint32_t* path,
                           size_t path_len,
                           const uint8_t* hash,
                           size_t hash_len,
                           uint8_t* sig,
                           size_t* sig_len) {
    if (sig_len == NULL || sig == NULL) {
        return CX_INVALID_PARAMETER;
    }
    if (*sig_len < ED25519_SIGNATURE_LENGTH) {
        return CX_INVALID_PARAMETER;
    }
    const mock_signature_data_t* entry = find_signature_entry(path, path_len, hash, hash_len);
    if (entry == NULL) {
        fprintf(stderr, "crypto_mock: missing signature path len=%zu [", path_len);
        for (size_t i = 0; i < path_len; i++) {
            fprintf(stderr, "%s0x%08x", (i == 0 ? "" : ", "), path[i]);
        }
        fprintf(stderr, "] message_len=%zu\n", hash_len);
        return CX_INTERNAL_ERROR;
    }

    memcpy(sig, entry->signature, ED25519_SIGNATURE_LENGTH);
    *sig_len = ED25519_SIGNATURE_LENGTH;
    return CX_OK;
}
