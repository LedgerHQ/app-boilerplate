#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <cmocka.h>

#include "globals.h"
#include "cardano_swo.h"
#include "transaction/tx_prepare.h"

static void reset_context(void) {
    explicit_bzero(&G_context, sizeof(G_context));
    G_context.req_type = REQUEST_SIGN_TRANSACTION;
}

static void test_compute_tx_hash_and_plan_ui_counts_ttl(void **state) {
    (void) state;
    reset_context();

    G_context.tx_info.transaction.includeTtl = true;
    G_context.tx_info.transaction.ttl = 123;

    tx_ui_plan_t plan = {0};
    int rc = compute_tx_hash_and_plan_ui(&plan);
    assert_int_equal(rc, SWO_SUCCESS);
    assert_int_equal(plan.pair_count, 3);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_compute_tx_hash_and_plan_ui_counts_ttl),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
