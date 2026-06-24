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

#ifdef HAVE_MLDSA

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "cx.h"
#include "lcx_mldsa.h"
#include "buffer.h"
#include "io.h"
#include "ledger_assert.h"

#include "mldsa.h"
#include "sw.h"
#include "globals.h"
#include "cx_mldsa_internal.h"

pqc_apdu_ctx_t G_pqc_ctx;


#define MLDSA_APDU_MAX_DATA_LEN 255

/* Parameter set identifiers (P2 bits 0-1) */
#define MLDSA_PARAM_44  0U
#define MLDSA_PARAM_65  1U
#define MLDSA_PARAM_87  2U

static void mldsa_reset_ctx(void) {
    explicit_bzero(&G_mldsa_ctx, sizeof(G_mldsa_ctx));
}

static bool mldsa_get_expected_lengths(uint8_t ins, uint8_t param_set,
                                       uint16_t *min_input, uint16_t *max_input,
                                       uint16_t *output_len) {
    uint16_t pk, sk, sig;

    if (param_set == MLDSA_PARAM_44) {
        pk  = MLDSA44_PUBLICKEYBYTES;
        sk  = MLDSA44_SECRETKEYBYTES;
        sig = MLDSA44_SIGBYTES;
    } else if (param_set == MLDSA_PARAM_65) {
        pk  = MLDSA65_PUBLICKEYBYTES;
        sk  = MLDSA65_SECRETKEYBYTES;
        sig = MLDSA65_SIGBYTES;
    }
#ifdef HAVE_MLDSA_87
    else if (param_set == MLDSA_PARAM_87) {
        pk  = MLDSA87_PUBLICKEYBYTES;
        sk  = MLDSA87_SECRETKEYBYTES;
        sig = MLDSA87_SIGBYTES;
    }
#endif // HAVE_MLDSA_87
    else {
        return false;
    }

    switch (ins) {
        case MLDSA_KEYGEN:
            *min_input = MLDSA_SEEDBYTES;
            *max_input = MLDSA_SEEDBYTES;
            *output_len = pk + sk;
            return true;
        case MLDSA_SIGN:
            *min_input = sk + MLDSA_CRHBYTES;
            *max_input = sk + MLDSA_CRHBYTES;
            *output_len = sig;
            return true;
        case MLDSA_VERIFY:
            *min_input = pk + sig + MLDSA_CRHBYTES;
            *max_input = pk + sig + MLDSA_CRHBYTES;
            *output_len = 1;
            return true;
        default:
            return false;
    }
}

static int mldsa_send_next_chunk(void) {
    uint8_t resp[MLDSA_APDU_MAX_DATA_LEN];
    uint16_t remaining = G_mldsa_ctx.out_len - G_mldsa_ctx.out_offset;
    uint16_t chunk_len;
    size_t offset = 0;

    if (remaining == 0) {
        mldsa_reset_ctx();
        return io_send_sw(SWO_CONDITIONS_NOT_SATISFIED);
    }

    if (!G_mldsa_ctx.out_header_sent) {
        if (G_mldsa_ctx.out_len > (uint16_t) (MLDSA_APDU_MAX_DATA_LEN - 2)) {
            chunk_len = MLDSA_APDU_MAX_DATA_LEN - 2;
        } else {
            chunk_len = G_mldsa_ctx.out_len;
        }

        resp[offset++] = (uint8_t) ((G_mldsa_ctx.out_len >> 8) & 0xFF);
        resp[offset++] = (uint8_t) (G_mldsa_ctx.out_len & 0xFF);
        G_mldsa_ctx.out_header_sent = true;
    } else {
        chunk_len = remaining > MLDSA_APDU_MAX_DATA_LEN ? MLDSA_APDU_MAX_DATA_LEN : remaining;
    }

    memmove(resp + offset, G_mldsa_ctx.buf + G_mldsa_ctx.out_offset, chunk_len);
    G_mldsa_ctx.out_offset += chunk_len;
    offset += chunk_len;

    if (G_mldsa_ctx.out_offset == G_mldsa_ctx.out_len) {
        int ret = io_send_response_pointer(resp, offset, SWO_SUCCESS);
        mldsa_reset_ctx();
        return ret;
    }

    return io_send_response_pointer(resp, offset, SWO_SUCCESS);
}

static int mldsa_finalize_operation(void) {
    int rc = 0;
    uint16_t pk_len, sk_len, sig_len;
    MLDSA_param_t mldsa_param;

    if (G_mldsa_ctx.param_set == MLDSA_PARAM_44) {
        pk_len  = MLDSA44_PUBLICKEYBYTES;
        sk_len  = MLDSA44_SECRETKEYBYTES;
        sig_len = MLDSA44_SIGBYTES;
        mldsa_param = MLDSA_44;
    }
#ifdef HAVE_MLDSA_87
    else if (G_mldsa_ctx.param_set == MLDSA_PARAM_87) {
        pk_len  = MLDSA87_PUBLICKEYBYTES;
        sk_len  = MLDSA87_SECRETKEYBYTES;
        sig_len = MLDSA87_SIGBYTES;
        mldsa_param = MLDSA_87;
    }
#endif // HAVE_MLDSA_87 
    else {
        pk_len  = MLDSA65_PUBLICKEYBYTES;
        sk_len  = MLDSA65_SECRETKEYBYTES;
        sig_len = MLDSA65_SIGBYTES;
        mldsa_param = MLDSA_65;
    }

    if (G_mldsa_ctx.ins == MLDSA_KEYGEN) {
        /* Seed (32 B) is at buf[0]; pk output also starts at buf[0] → copy to stack */
        uint8_t seed_copy[MLDSA_SEEDBYTES];
        memcpy(seed_copy, G_mldsa_ctx.buf, MLDSA_SEEDBYTES);
        rc = MLDSA_internal_keygen(G_mldsa_ctx.buf,
                                    MLDSA_APDU_BUF_SIZE,
                                    G_mldsa_ctx.buf + pk_len,
                                    MLDSA_APDU_BUF_SIZE - pk_len,
                                    seed_copy, mldsa_param);
        explicit_bzero(seed_copy, sizeof(seed_copy));
    } else if (G_mldsa_ctx.ins == MLDSA_SIGN) {
        size_t siglen = 0;
        /* mu (64 B) sits right after sk in buf and would be overwritten by sig
         * output placed at buf[sk_len].  Copy it to stack first. */
        uint8_t mu_copy[MLDSA_CRHBYTES];
        memcpy(mu_copy, G_mldsa_ctx.buf + sk_len, MLDSA_CRHBYTES);
        rc = MLDSA_internal_sign(G_mldsa_ctx.buf + sk_len, // sig after sk
                                 MLDSA_APDU_BUF_SIZE - sk_len,
                                 &siglen,
                                 mu_copy, NULL, 0,
                                 G_mldsa_ctx.buf, sk_len,
                                 mldsa_param);
        explicit_bzero(mu_copy, sizeof(mu_copy));
        if (rc == 0) {
            /* Move sig from buf[sk_len..] to buf[0..] for chunk sending */
            memmove(G_mldsa_ctx.buf, G_mldsa_ctx.buf + sk_len, siglen);
            G_mldsa_ctx.out_len = (uint16_t) siglen;
        }
    } else if (G_mldsa_ctx.ins == MLDSA_VERIFY) {
        rc = MLDSA_internal_verify(G_mldsa_ctx.buf + pk_len, //sig
                                   sig_len,
                             G_mldsa_ctx.buf + pk_len + sig_len, // mu
                                   G_mldsa_ctx.buf, // pk
                                   pk_len,
                                   mldsa_param);
        if (rc == 0) {
            G_mldsa_ctx.buf[0] = 0x00; /* valid */
        } else {
            G_mldsa_ctx.buf[0] = 0x01; /* invalid */
            rc = 0; /* Not an error, just invalid signature */
        }
    } else {
        mldsa_reset_ctx();
        return io_send_sw(SWO_INVALID_INS);
    }

    if (rc != 0) {
        mldsa_reset_ctx();
        return io_send_sw(SWO_UNKNOWN);
    }

    return mldsa_send_next_chunk();
}

int handler_mldsa(buffer_t *cdata, uint8_t ins, uint8_t chunk, uint8_t p2) {
    uint16_t min_input_len;
    uint16_t max_input_len;
    uint16_t expected_output_len;
    bool more = (p2 & 0x80) != 0;
    uint8_t param_set = p2 & 0x03U;

    /* ML-DSA-44 (0), ML-DSA-65 (1), and ML-DSA-87 (2) are supported */
    if (param_set > MLDSA_PARAM_87) {
        mldsa_reset_ctx();
        return io_send_sw(SWO_INCORRECT_P1_P2);
    }

    if (!mldsa_get_expected_lengths(ins, param_set, &min_input_len, &max_input_len, &expected_output_len)) {
        mldsa_reset_ctx();
        return io_send_sw(SWO_INVALID_INS);
    }

    if (G_mldsa_ctx.active && G_mldsa_ctx.ins != ins) {
        mldsa_reset_ctx();
        return io_send_sw(SWO_CONDITIONS_NOT_SATISFIED);
    }

    /* Parameter set must be consistent within a session */
    if (G_mldsa_ctx.active && chunk != 0 && G_mldsa_ctx.param_set != param_set) {
        mldsa_reset_ctx();
        return io_send_sw(SWO_INCORRECT_P1_P2);
    }

    if (G_mldsa_ctx.active && G_mldsa_ctx.out_header_sent && G_mldsa_ctx.out_offset < G_mldsa_ctx.out_len) {
        if (cdata->size != 0 || more) {
            return io_send_sw(SWO_INCORRECT_P1_P2);
        }

        if (chunk != G_mldsa_ctx.expected_chunk) {
            return io_send_sw(SWO_INCORRECT_P1_P2);
        }

        G_mldsa_ctx.expected_chunk++;
        return mldsa_send_next_chunk();
    }

    if (chunk == 0) {
        mldsa_reset_ctx();
        G_mldsa_ctx.active = true;
        G_mldsa_ctx.ins = ins;
        G_mldsa_ctx.param_set = param_set;
        G_mldsa_ctx.expected_chunk = 0;
        G_mldsa_ctx.out_len = expected_output_len;
    }

    if (!G_mldsa_ctx.active) {
        return io_send_sw(SWO_CONDITIONS_NOT_SATISFIED);
    }

    if (chunk != G_mldsa_ctx.expected_chunk) {
        mldsa_reset_ctx();
        return io_send_sw(SWO_INCORRECT_P1_P2);
    }

    if (cdata->size == 0) {
        mldsa_reset_ctx();
        return io_send_sw(SWO_WRONG_DATA_LENGTH);
    }

    if (G_mldsa_ctx.in_len + cdata->size > MLDSA_APDU_BUF_SIZE) {
        mldsa_reset_ctx();
        return io_send_sw(SWO_WRONG_DATA_LENGTH);
    }

    if (!buffer_move(cdata, G_mldsa_ctx.buf + G_mldsa_ctx.in_len, cdata->size)) {
        mldsa_reset_ctx();
        return io_send_sw(SWO_INCORRECT_DATA);
    }

    G_mldsa_ctx.in_len += cdata->size;
    G_mldsa_ctx.expected_chunk++;

    if (more) {
        if (G_mldsa_ctx.in_len >= max_input_len) {
            mldsa_reset_ctx();
            return io_send_sw(SWO_WRONG_DATA_LENGTH);
        }
        return io_send_sw(SWO_SUCCESS);
    }

    if (G_mldsa_ctx.in_len < min_input_len || G_mldsa_ctx.in_len > max_input_len) {
        mldsa_reset_ctx();
        return io_send_sw(SWO_WRONG_DATA_LENGTH);
    }

    return mldsa_finalize_operation();
}

#endif /* HAVE_MLDSA */
