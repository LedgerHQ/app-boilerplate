#include <stdbool.h>
#include "bench.h"

static bool is_prime(uint32_t num) {
    if (num <= 1) return false;

    for (uint32_t i = 2; (i * i) <= num; ++i) {
        if ((num % i) == 0) return false;
    }
    return true;
}

uint32_t bench_prime(uint32_t max_count) {
    uint32_t last = 0;
    uint32_t count = 0;
    uint32_t i;

    for (i = 0; count < max_count; ++i) {
        if (is_prime(i)) {
            count += 1;
            last = i;
        }
    }
    return last;
}

void bench_fibonacci(uint32_t max_count) {
    uint32_t fn_1 = 0;
    uint32_t fn = 1;
    uint32_t tmp;
    uint32_t count = 0;

    while (count < max_count) {
        tmp = fn_1 + fn;
        fn_1 = fn;
        fn = tmp;
        count += 1;
    }
}
