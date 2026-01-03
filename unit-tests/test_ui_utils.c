#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>

#include <cmocka.h>

#include "memory/mem.h"
#include "ui/ui_utils.h"

static void test_ui_pairs_add_static_label_stores_value(void **state) {
    (void) state;
    assert_true(ui_pairs_init(1));

    char *tmp = (char *) app_mem_alloc(16);
    assert_non_null(tmp);
    memcpy(tmp, "hello", sizeof("hello"));

    assert_true(ui_pairs_add_static_label("Label", tmp));
    assert_string_equal(g_pairs[0].item, "Label");
    assert_string_equal(g_pairs[0].value, "hello");

    ui_pairs_cleanup();
    ui_cleanup_tracked_allocations();
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_ui_pairs_add_static_label_stores_value),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
