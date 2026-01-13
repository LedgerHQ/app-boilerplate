#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <cmocka.h>

#include "utils/textUtils.h"

// Test ASCII validation functions
static void test_is_printable_ascii(void **state) {
    (void) state;

    // Valid printable ASCII without spaces
    const uint8_t *valid_no_spaces = (const uint8_t *) "HelloWorld123";
    assert_true(str_isPrintableAsciiWithoutSpaces(valid_no_spaces, strlen((const char *) valid_no_spaces)));

    // Valid printable ASCII with spaces
    const uint8_t *valid_with_spaces = (const uint8_t *) "Hello World 123";
    assert_true(str_isPrintableAsciiWithSpaces(valid_with_spaces, strlen((const char *) valid_with_spaces)));

    // Invalid - has space but checking without spaces allowed
    assert_false(str_isPrintableAsciiWithoutSpaces(valid_with_spaces, strlen((const char *) valid_with_spaces)));

    // Invalid - non-ASCII character
    const uint8_t invalid_ascii[] = {0x48, 0x65, 0xff, 0x00};  // "He" + invalid byte
    assert_false(str_isPrintableAsciiWithoutSpaces(invalid_ascii, 3));
    assert_false(str_isPrintableAsciiWithSpaces(invalid_ascii, 3));
}

// Test unambiguous ASCII
static void test_is_unambiguous_ascii(void **state) {
    (void) state;

    // Valid unambiguous ASCII - alphanumeric only
    const uint8_t *valid = (const uint8_t *) "HelloWorld123abc";
    assert_true(str_isUnambiguousAscii(valid, strlen((const char *) valid)));

    // Valid - single spaces in middle allowed
    const uint8_t *with_space = (const uint8_t *) "Hello World";
    assert_true(str_isUnambiguousAscii(with_space, strlen((const char *) with_space)));

    // Valid - printable special characters allowed (hyphen, etc)
    const uint8_t *with_special = (const uint8_t *) "Hello-World";
    assert_true(str_isUnambiguousAscii(with_special, strlen((const char *) with_special)));

    // Invalid - has leading space
    const uint8_t *leading_space = (const uint8_t *) " Hello";
    assert_false(str_isUnambiguousAscii(leading_space, strlen((const char *) leading_space)));

    // Invalid - has trailing space
    const uint8_t *trailing_space = (const uint8_t *) "Hello ";
    assert_false(str_isUnambiguousAscii(trailing_space, strlen((const char *) trailing_space)));

    // Invalid - double space
    const uint8_t *double_space = (const uint8_t *) "Hello  World";
    assert_false(str_isUnambiguousAscii(double_space, strlen((const char *) double_space)));

    // Invalid - has non-ASCII
    const uint8_t non_ascii[] = {0x48, 0x65, 0xff, 0x00};
    assert_false(str_isUnambiguousAscii(non_ascii, 3));

    // Invalid - empty string
    assert_false(str_isUnambiguousAscii((const uint8_t *) "", 0));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_is_printable_ascii),
        cmocka_unit_test(test_is_unambiguous_ascii),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
