/*****************************************************************************
 *   Ledger App Boilerplate.
 *   (c) 2020 Ledger SAS.
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

#include "pqc_ctx.h"

#ifdef HAVE_MLKEM

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "cx.h"
#include "lcx_mlkem.h"
#include "buffer.h"
#include "io.h"
#include "ledger_assert.h"

#include "mlkem.h"
#include "sw.h"
#include "globals.h"
#include "cx_mlkem_internal.h"

/* When ML-DSA is excluded from the build, mldsa.c does not provide the shared
 * PQC context, so define it here. */
#if !defined(HAVE_MLDSA)
pqc_apdu_ctx_t G_pqc_ctx;
#endif

#define MLKEM_APDU_MAX_DATA_LEN 255

/* Parameter set identifiers (P2 bits 0-1) */
#define MLKEM_PARAM_512  0U
#define MLKEM_PARAM_768  1U
#define MLKEM_PARAM_1024 2U

/* Coins sizes */
#define MLKEM_KEYGEN_COINS (2U * MLKEM_SYMBYTES) /* d || z = 64 bytes */
#define MLKEM_ENCAP_COINS  MLKEM_SYMBYTES         /* m = 32 bytes      */

static void mlkem_reset_ctx(void) {
    explicit_bzero(&G_mlkem_ctx, sizeof(G_mlkem_ctx));
}

static bool mlkem_get_expected_lengths(uint8_t ins, uint8_t param_set,
                                       uint16_t *min_input, uint16_t *max_input,
                                       uint16_t *output_len) {
    uint16_t pk, sk, ct;

    if (param_set == MLKEM_PARAM_512) {
        pk = MLKEM512_PUBLICKEYBYTES;
        sk = MLKEM512_SECRETKEYBYTES;
        ct = MLKEM512_CIPHERTEXTBYTES;
    } else if (param_set == MLKEM_PARAM_768) {
        pk = MLKEM768_PUBLICKEYBYTES;
        sk = MLKEM768_SECRETKEYBYTES;
        ct = MLKEM768_CIPHERTEXTBYTES;
    } else if (param_set == MLKEM_PARAM_1024) {
        pk = MLKEM1024_PUBLICKEYBYTES;
        sk = MLKEM1024_SECRETKEYBYTES;
        ct = MLKEM1024_CIPHERTEXTBYTES;
    } else {
        return false;
    }

    switch (ins) {
        case MLKEM_KEYGEN:
            *min_input = MLKEM_KEYGEN_COINS;
            *max_input = MLKEM_KEYGEN_COINS;
            *output_len = pk + sk;
            return true;
        case MLKEM_ENCAPSULATE:
            *min_input = pk + MLKEM_ENCAP_COINS;
            *max_input = pk + MLKEM_ENCAP_COINS;
            *output_len = ct + MLKEM_SSBYTES;
            return true;
        case MLKEM_DECAPSULATE:
            *min_input = ct + sk;
            *max_input = ct + sk;
            *output_len = MLKEM_SSBYTES;
            return true;
        default:
            return false;
    }
}

static int mlkem_send_next_chunk(void) {
    uint8_t resp[MLKEM_APDU_MAX_DATA_LEN];
    uint16_t remaining = G_mlkem_ctx.out_len - G_mlkem_ctx.out_offset;
    uint16_t chunk_len;
    size_t offset = 0;

    if (remaining == 0) {
        mlkem_reset_ctx();
        return io_send_sw(SWO_CONDITIONS_NOT_SATISFIED);
    }

    if (!G_mlkem_ctx.out_header_sent) {
        if (G_mlkem_ctx.out_len > (uint16_t) (MLKEM_APDU_MAX_DATA_LEN - 2)) {
            chunk_len = MLKEM_APDU_MAX_DATA_LEN - 2;
        } else {
            chunk_len = G_mlkem_ctx.out_len;
        }

        resp[offset++] = (uint8_t) ((G_mlkem_ctx.out_len >> 8) & 0xFF);
        resp[offset++] = (uint8_t) (G_mlkem_ctx.out_len & 0xFF);
        G_mlkem_ctx.out_header_sent = true;
    } else {
        chunk_len = remaining > MLKEM_APDU_MAX_DATA_LEN ? MLKEM_APDU_MAX_DATA_LEN : remaining;
    }

    memmove(resp + offset, G_mlkem_ctx.buf + G_mlkem_ctx.out_offset, chunk_len);
    G_mlkem_ctx.out_offset += chunk_len;
    offset += chunk_len;

    if (G_mlkem_ctx.out_offset == G_mlkem_ctx.out_len) {
        int ret = io_send_response_pointer(resp, offset, SWO_SUCCESS);
        mlkem_reset_ctx();
        return ret;
    }

    return io_send_response_pointer(resp, offset, SWO_SUCCESS);
}

static int mlkem_finalize_operation(void) {
    cx_err_t rc;
    uint16_t pk_len, sk_len, ct_len;
    MLKEM_param_t mlkem_param;

    if (G_mlkem_ctx.param_set == MLKEM_PARAM_512) {
        pk_len  = MLKEM512_PUBLICKEYBYTES;
        sk_len  = MLKEM512_SECRETKEYBYTES;
        ct_len  = MLKEM512_CIPHERTEXTBYTES;
        mlkem_param = MLKEM_512;
    } else if (G_mlkem_ctx.param_set == MLKEM_PARAM_768) {
        pk_len  = MLKEM768_PUBLICKEYBYTES;
        sk_len  = MLKEM768_SECRETKEYBYTES;
        ct_len  = MLKEM768_CIPHERTEXTBYTES;
        mlkem_param = MLKEM_768;
    } else if (G_mlkem_ctx.param_set == MLKEM_PARAM_1024) {
        pk_len  = MLKEM1024_PUBLICKEYBYTES;
        sk_len  = MLKEM1024_SECRETKEYBYTES;
        ct_len  = MLKEM1024_CIPHERTEXTBYTES;
        mlkem_param = MLKEM_1024;
    } else {
        mlkem_reset_ctx();
        return io_send_sw(SWO_INCORRECT_P1_P2);
    }

    if (G_mlkem_ctx.ins == MLKEM_KEYGEN) {
        /* Coins (64 B) at buf[0] overlap pk output at buf[0] → copy to stack */
        uint8_t coins_copy[MLKEM_KEYGEN_COINS];
        memcpy(coins_copy, G_mlkem_ctx.buf, MLKEM_KEYGEN_COINS);
        rc = MLKEM_crypto_kem_keypair_derand(G_mlkem_ctx.buf,
                                             MLKEM_APDU_BUF_SIZE,
                                             G_mlkem_ctx.buf + pk_len,
                                             MLKEM_APDU_BUF_SIZE - pk_len,
                                             coins_copy, mlkem_param);
        explicit_bzero(coins_copy, sizeof(coins_copy));
    } else if (G_mlkem_ctx.ins == MLKEM_ENCAPSULATE) {
        /* buf layout: [pk (pk_len) | m (32)]
         * Copy m to stack, write ct after pk in buf, ss to stack. */
        uint8_t m_copy[MLKEM_ENCAP_COINS];
        uint8_t ss[MLKEM_SSBYTES];
        memcpy(m_copy, G_mlkem_ctx.buf + pk_len, MLKEM_ENCAP_COINS);
        rc = MLKEM_crypto_kem_enc_derand(G_mlkem_ctx.buf + pk_len,
                                         MLKEM_APDU_BUF_SIZE - pk_len,
                                         ss, sizeof(ss),
                                         G_mlkem_ctx.buf, pk_len,
                                         m_copy, mlkem_param);
        explicit_bzero(m_copy, sizeof(m_copy));
        if (rc == CX_OK) {
            /* Move ct from buf[pk_len] to buf[0], append ss */
            memmove(G_mlkem_ctx.buf, G_mlkem_ctx.buf + pk_len, ct_len);
            memcpy(G_mlkem_ctx.buf + ct_len, ss, MLKEM_SSBYTES);
        }
        explicit_bzero(ss, sizeof(ss));
    } else if (G_mlkem_ctx.ins == MLKEM_DECAPSULATE) {
        /* buf layout: [ct (ct_len) | sk (sk_len)]
         * Write ss to stack to avoid aliasing with ct input. */
        uint8_t ss[MLKEM_SSBYTES];
        rc = MLKEM_crypto_kem_dec(ss, sizeof(ss),
                                  G_mlkem_ctx.buf, ct_len,
                                  G_mlkem_ctx.buf + ct_len, sk_len,
                                  mlkem_param);
        if (rc == CX_OK) {
            memcpy(G_mlkem_ctx.buf, ss, MLKEM_SSBYTES);
        }
        explicit_bzero(ss, sizeof(ss));
    } else {
        mlkem_reset_ctx();
        return io_send_sw(SWO_INVALID_INS);
    }

    if (rc != CX_OK) {
        mlkem_reset_ctx();
        return io_send_sw(SWO_UNKNOWN);
    }

    return mlkem_send_next_chunk();
}

int handler_mlkem(buffer_t *cdata, uint8_t ins, uint8_t chunk, uint8_t p2) {
    uint16_t min_input_len;
    uint16_t max_input_len;
    uint16_t expected_output_len;
    bool more = (p2 & 0x80) != 0;
    uint8_t param_set = p2 & 0x03U;

    /* ML-KEM-512 (0), ML-KEM-768 (1), ML-KEM-1024 (2) */
    if (param_set > MLKEM_PARAM_1024) {
        mlkem_reset_ctx();
        return io_send_sw(SWO_INCORRECT_P1_P2);
    }

    if (!mlkem_get_expected_lengths(ins, param_set, &min_input_len, &max_input_len, &expected_output_len)) {
        mlkem_reset_ctx();
        return io_send_sw(SWO_INVALID_INS);
    }

    if (G_mlkem_ctx.active && G_mlkem_ctx.ins != ins) {
        mlkem_reset_ctx();
        return io_send_sw(SWO_CONDITIONS_NOT_SATISFIED);
    }

    /* Parameter set must be consistent within a session */
    if (G_mlkem_ctx.active && chunk != 0 && G_mlkem_ctx.param_set != param_set) {
        mlkem_reset_ctx();
        return io_send_sw(SWO_INCORRECT_P1_P2);
    }

    if (G_mlkem_ctx.active && G_mlkem_ctx.out_header_sent && G_mlkem_ctx.out_offset < G_mlkem_ctx.out_len) {
        if (cdata->size != 0 || more) {
            return io_send_sw(SWO_INCORRECT_P1_P2);
        }

        if (chunk != G_mlkem_ctx.expected_chunk) {
            return io_send_sw(SWO_INCORRECT_P1_P2);
        }

        G_mlkem_ctx.expected_chunk++;
        return mlkem_send_next_chunk();
    }

    if (chunk == 0) {
        mlkem_reset_ctx();
        G_mlkem_ctx.active = true;
        G_mlkem_ctx.ins = ins;
        G_mlkem_ctx.param_set = param_set;
        G_mlkem_ctx.expected_chunk = 0;
        G_mlkem_ctx.out_len = expected_output_len;
    }

    if (!G_mlkem_ctx.active) {
        return io_send_sw(SWO_CONDITIONS_NOT_SATISFIED);
    }

    if (chunk != G_mlkem_ctx.expected_chunk) {
        mlkem_reset_ctx();
        return io_send_sw(SWO_INCORRECT_P1_P2);
    }

    if (cdata->size == 0) {
        mlkem_reset_ctx();
        return io_send_sw(SWO_WRONG_DATA_LENGTH);
    }

    if (G_mlkem_ctx.in_len + cdata->size > MLKEM_APDU_BUF_SIZE) {
        mlkem_reset_ctx();
        return io_send_sw(SWO_WRONG_DATA_LENGTH);
    }

    if (!buffer_move(cdata, G_mlkem_ctx.buf + G_mlkem_ctx.in_len, cdata->size)) {
        mlkem_reset_ctx();
        return io_send_sw(SWO_INCORRECT_DATA);
    }

    G_mlkem_ctx.in_len += cdata->size;
    G_mlkem_ctx.expected_chunk++;

    if (more) {
        if (G_mlkem_ctx.in_len >= max_input_len) {
            mlkem_reset_ctx();
            return io_send_sw(SWO_WRONG_DATA_LENGTH);
        }
        return io_send_sw(SWO_SUCCESS);
    }

    if (G_mlkem_ctx.in_len < min_input_len || G_mlkem_ctx.in_len > max_input_len) {
        mlkem_reset_ctx();
        return io_send_sw(SWO_WRONG_DATA_LENGTH);
    }

    return mlkem_finalize_operation();
}

#endif /* HAVE_MLKEM */
