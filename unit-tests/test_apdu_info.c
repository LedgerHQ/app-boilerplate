#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <cmocka.h>

#include "cardano_swo.h"
#include "handler/get_app_name.h"
#include "handler/get_serial.h"
#include "handler/get_version.h"

static uint8_t g_response_buffer[64];
static size_t g_response_length;
static uint16_t g_response_sw;

int io_send_response_pointer(const uint8_t* buffer, size_t bufferLength, uint16_t swo) {
    assert_true(bufferLength <= sizeof(g_response_buffer));
    memcpy(g_response_buffer, buffer, bufferLength);
    g_response_length = bufferLength;
    g_response_sw = swo;
    return 0;
}

unsigned int os_serial(unsigned char* serial, unsigned int maxlength) {
    static const unsigned char MOCK_SERIAL[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x12, 0x34};
    assert_int_equal(maxlength, sizeof(MOCK_SERIAL));
    memcpy(serial, MOCK_SERIAL, sizeof(MOCK_SERIAL));
    return sizeof(MOCK_SERIAL);
}

static void reset_response(void) {
    g_response_length = 0;
    g_response_sw = 0;
    memset(g_response_buffer, 0, sizeof(g_response_buffer));
}

static void test_get_version_returns_current_constants(void** state) {
    (void) state;
    reset_response();

    handler_get_version();
    assert_int_equal(g_response_sw, SWO_SUCCESS);
    assert_int_equal(g_response_length, APPVERSION_LEN);

    const uint8_t expected[] = {(uint8_t) MAJOR_VERSION,
                                (uint8_t) MINOR_VERSION,
                                (uint8_t) PATCH_VERSION};
    assert_memory_equal(g_response_buffer, expected, sizeof(expected));
}

static void test_get_app_name_returns_literal(void** state) {
    (void) state;
    reset_response();

    handler_get_app_name();
    assert_int_equal(g_response_sw, SWO_SUCCESS);
    assert_int_equal(g_response_length, APPNAME_LEN);
    assert_memory_equal(g_response_buffer, APPNAME, APPNAME_LEN);
}

static void test_get_serial_returns_os_value(void** state) {
    (void) state;
    reset_response();

    handler_get_serial();
    assert_int_equal(g_response_sw, SWO_SUCCESS);
    const unsigned char expected[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x12, 0x34};
    assert_int_equal(g_response_length, sizeof(expected));
    assert_memory_equal(g_response_buffer, expected, sizeof(expected));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_get_version_returns_current_constants),
        cmocka_unit_test(test_get_app_name_returns_literal),
        cmocka_unit_test(test_get_serial_returns_os_value),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
