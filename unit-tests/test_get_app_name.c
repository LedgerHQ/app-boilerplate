/*
 * Unit tests for src/handler/get_app_name.c.
 *
 * handler_get_app_name() sends APPNAME via io_send_response_pointer (a static
 * inline wrapper), so the mockable symbol is io_send_response_buffers. A
 * capture callback lets us assert the payload and status word.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity.h"

#include "Mockio.h"

#include "get_app_name.h"
#include "constants.h"  // APPNAME_LEN
#include "sw.h"

static uint8_t g_resp[MAX_APPNAME_LEN];
static size_t g_resp_size;
static size_t g_count;
static uint16_t g_sw;

static int capture_cb(const buffer_t *rdatalist, size_t count, uint16_t sw, int cmock_num_calls) {
    (void) cmock_num_calls;
    g_count = count;
    g_sw = sw;
    g_resp_size = rdatalist->size;
    memcpy(g_resp, rdatalist->ptr, rdatalist->size);
    return 0;
}

void setUp(void) {
    Mockio_Init();
    memset(g_resp, 0, sizeof(g_resp));
    g_resp_size = 0;
    g_count = 0;
    g_sw = 0;
}

void tearDown(void) {
    Mockio_Verify();
    Mockio_Destroy();
}

void test_get_app_name(void) {
    io_send_response_buffers_AddCallback(capture_cb);
    io_send_response_buffers_ExpectAnyArgsAndReturn(0);

    int ret = handler_get_app_name();

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(1, g_count);
    TEST_ASSERT_EQUAL_HEX16(SWO_SUCCESS, g_sw);
    TEST_ASSERT_EQUAL(APPNAME_LEN, g_resp_size);
    TEST_ASSERT_EQUAL_MEMORY(APPNAME, g_resp, APPNAME_LEN);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_get_app_name);
    return UNITY_END();
}
