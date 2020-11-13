#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>

#include <cmocka.h>

#include "common/bip32.h"

static void test_bip32(void **state) {
    (void) state;

    char output[30];
    bool b = false;

    b = bip32_path_to_str((const uint32_t[5]){0x8000002C, 0x80000000, 0x80000000, 0, 0},
                          5,
                          output,
                          sizeof(output));
    assert_true(b);
    assert_string_equal(output, "44'/0'/0'/0/0");

    b = bip32_path_to_str((const uint32_t[5]){0x8000002C, 0x80000001, 0x80000000, 0, 0},
                          5,
                          output,
                          sizeof(output));
    assert_true(b);
    assert_string_equal(output, "44'/1'/0'/0/0");
}

static void test_bad_bip32(void **state) {
    (void) state;

    char output[30];
    bool b = true;

    // More than MAX_BIP32_PATH (=10)
    b = bip32_path_to_str(
        (const uint32_t[11]){0x8000002C, 0x80000000, 0x80000000, 0, 0, 0, 0, 0, 0, 0, 0},
        11,
        output,
        sizeof(output));
    assert_false(b);

    // No BIP32 path (=0)
    b = bip32_path_to_str(NULL, 0, output, sizeof(output));
    assert_false(b);
}

static void test_bip32_parsing(void **state) {
    (void) state;

    // clang-format off
    uint8_t buffer[20] = {
        0x80, 0x00, 0x00, 0x2C,
        0x80, 0x00, 0x00, 0x01,
        0x80, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    uint32_t expected[5] = {0x8000002C, 0x80000001, 0x80000000, 0, 0};
    uint32_t output[5] = {0};
    bool b = false;
    buffer_t buf = {.ptr = buffer, .size = sizeof(buffer), .offset = 0};

    b = bip32_path_from_buffer(&buf, output, 5);
    assert_true(b);
    assert_memory_equal(output, expected, 5);
}

static void test_bad_bip32_parsing(void **state) {
    (void) state;

    // clang-format off
    uint8_t buffer[20] = {
        0x80, 0x00, 0x00, 0x2C,
        0x80, 0x00, 0x00, 0x01,
        0x80, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    uint32_t output[10] = {0};

    buffer_t buf = {.ptr = buffer, .size = sizeof(buffer), .offset = 0};

    // buffer too small (5 BIP32 paths instead of 10)
    assert_false(bip32_path_from_buffer(&buf, output, 10));

    // No BIP32 path
    assert_false(bip32_path_from_buffer(&buf, output, 0));

    // More than MAX_BIP32_PATH (=10)
    assert_false(bip32_path_from_buffer(&buf, output, 20));
}

int main() {
    const struct CMUnitTest tests[] = {cmocka_unit_test(test_bip32),
                                       cmocka_unit_test(test_bad_bip32),
                                       cmocka_unit_test(test_bip32_parsing),
                                       cmocka_unit_test(test_bad_bip32_parsing)};

    return cmocka_run_group_tests(tests, NULL, NULL);
}
