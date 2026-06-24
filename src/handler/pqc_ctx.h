#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef HAVE_MLDSA
/* ML-DSA buffer: max across all param sets of
 *   keygen out = pk + sk,  sign in+out = sk + sig,  verify in = pk + sig + mu.
 * ML-DSA-87 sign: sk(4896) + sig(4627) = 9523 is the largest. */
#define MLDSA_APDU_BUF_SIZE 9523U

typedef struct {
    bool active;
    uint8_t ins;
    uint8_t param_set;
    uint8_t expected_chunk;
    uint16_t in_len;
    uint16_t out_len;
    uint16_t out_offset;
    bool out_header_sent;
    uint8_t buf[MLDSA_APDU_BUF_SIZE];
} mldsa_apdu_ctx_t;
#endif /* HAVE_MLDSA */

#ifdef HAVE_MLKEM
/* ML-KEM buffer: max across all param sets of
 *   keygen out = pk + sk, encap in = pk + coins, decap in = ct + sk.
 * ML-KEM-1024 keygen/decap: pk(1568) + sk(3168) = ct(1568) + sk(3168) = 4736. */
#define MLKEM_APDU_BUF_SIZE 4736U

typedef struct {
    bool active;
    uint8_t ins;
    uint8_t param_set;
    uint8_t expected_chunk;
    uint16_t in_len;
    uint16_t out_len;
    uint16_t out_offset;
    bool out_header_sent;
    uint8_t buf[MLKEM_APDU_BUF_SIZE];
} mlkem_apdu_ctx_t;
#endif /* HAVE_MLKEM */

typedef union {
#ifdef HAVE_MLDSA
    mldsa_apdu_ctx_t mldsa;
#endif
#ifdef HAVE_MLKEM
    mlkem_apdu_ctx_t mlkem;
#endif
} pqc_apdu_ctx_t;

extern pqc_apdu_ctx_t G_pqc_ctx;

#ifdef HAVE_MLDSA
#define G_mldsa_ctx (G_pqc_ctx.mldsa)
#endif
#ifdef HAVE_MLKEM
#define G_mlkem_ctx (G_pqc_ctx.mlkem)
#endif
