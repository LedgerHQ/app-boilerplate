// SHA3-256 reference implementation
// Based on public domain Keccak code from https://keccak.team/

#include "sha3-256.h"
#include <string.h>

// Keccak constants
#define KECCAK_ROUNDS 24
#define HASH_LEN 32  // SHA3-256
#define RATE (200 - 2 * HASH_LEN)  // 136 bytes

static const uint64_t keccak_round_constants[KECCAK_ROUNDS] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000008000800aULL, 0x000000008000000aULL,
    0x0000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008081ULL,
    0x8000000000008009ULL, 0x000000000000008aULL, 0x0000000000000088ULL,
    0x0000000080000001ULL, 0x000000008000008bULL, 0x000000008000008aULL,
    0x00000001000000ULL, 0x8000000080000081ULL, 0x8000000000000080ULL,
    0x80000001000000ULL, 0x8000000080008008ULL, 0x0000000000000084ULL,
    0x0000000080000080ULL, 0x00000001800000ULL, 0x000000008000000aULL
};

static inline uint64_t rol(uint64_t x, int n) {
    return (x << n) | (x >> (64 - n));
}

static void keccak_f(uint64_t *a) {
    int i, j, r;
    uint64_t b[25], c[5], d, x, y;

    for (r = 0; r < KECCAK_ROUNDS; r++) {
        // Theta
        for (i = 0; i < 5; i++) {
            c[i] = a[i] ^ a[i + 5] ^ a[i + 10] ^ a[i + 15] ^ a[i + 20];
        }
        for (i = 0; i < 5; i++) {
            d = c[(i + 4) % 5] ^ rol(c[(i + 1) % 5], 1);
            for (j = 0; j < 25; j += 5) {
                a[i + j] ^= d;
            }
        }

        // Rho and Pi
        b[0] = a[0];
        x = 1;
        y = 0;
        for (i = 0; i < 24; i++) {
            int t = (x + 5 * y) % 25;
            b[t] = rol(a[5 * y + ((x + 3 * y) % 5)], ((i + 1) * (i + 2) / 2) % 64);
            int tx = x;
            x = y;
            y = (2 * tx + 3 * y) % 5;
        }
        memcpy(a, b, sizeof(b));

        // Chi
        for (j = 0; j < 25; j += 5) {
            for (i = 0; i < 5; i++) {
                c[i] = a[i + j];
            }
            for (i = 0; i < 5; i++) {
                a[i + j] ^= (~c[(i + 1) % 5]) & c[(i + 2) % 5];
            }
        }

        // Iota
        a[0] ^= keccak_round_constants[r];
    }
}

void calc_sha3_256(uint8_t *hash, const uint8_t *data, size_t len) {
    uint64_t a[25] = {0};
    uint8_t *aBytes = (uint8_t *)a;
    size_t blockSize = RATE;
    size_t absorbed = 0;

    // Absorb phase
    while (absorbed + blockSize <= len) {
        for (size_t i = 0; i < blockSize; i++) {
            aBytes[i] ^= data[absorbed + i];
        }
        keccak_f(a);
        absorbed += blockSize;
    }

    // Absorb remaining data
    size_t remaining = len - absorbed;
    for (size_t i = 0; i < remaining; i++) {
        aBytes[i] ^= data[absorbed + i];
    }

    // Domain separation for SHA3 (0x06)
    aBytes[remaining] ^= 0x06;
    aBytes[blockSize - 1] ^= 0x80;

    // Final permutation
    keccak_f(a);

    // Squeeze phase
    memcpy(hash, aBytes, HASH_LEN);
}
