/*
 * Unit tests for src/handler/provide_token_info.c.
 *
 * handler_provide_token_info() validates a CAL dynamic-token TLV descriptor,
 * its coin type and TUID sub-TLV, then stores the token. Dependencies mocked:
 *   - tlv_use_case_dynamic_descriptor (PKI use case)
 *   - _parse_tlv_internal / tlv_check_received_tags / get_buffer_from_tlv_data
 *     (the engine behind the macro-generated TUID parser)
 *   - io_send_response_buffers (io_send_sw)
 * set_token_info / init_dynamic_token_storage are hand-stubbed
 * (dynamic_token_info.h is not CMock-mockable: set_token_info has a char(*)[N]
 * parameter the generator cannot parse).
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "unity.h"

#include "Mocktlv_use_case_dynamic_descriptor.h"
#include "Mocktlv_library.h"
#include "Mockio.h"

#include "provide_token_info.h"
#include "dynamic_token_info.h"  // set_token_info / init_dynamic_token_storage (stubbed)
#include "constants.h"           // BOILERPLATE_SLIP44_COIN_TYPE
#include "sw.h"

// ---- control knobs / captures ----
static tlv_dynamic_descriptor_status_t g_tlv_status;
static uint32_t g_coin_type;
static bool g_tuid_parse_ret;
static bool g_tags_ok;
static uint16_t g_last_sw;
static bool g_set_token_called;
static bool g_invoke_handler;  // make the parser mock call the TUID tag handler

// ---- hand-stubs (dynamic_token_info.h not mockable) ----
void init_dynamic_token_storage(void) {
}
void set_token_info(uint8_t decimals,
                    char (*ticker)[MAX_TICKER_SIZE + 1],
                    const buffer_t *token_address_buffer) {
    (void) decimals;
    (void) ticker;
    (void) token_address_buffer;
    g_set_token_called = true;
}

// ---- mock callbacks ----
static tlv_dynamic_descriptor_status_t tlv_uc_cb(const buffer_t *payload,
                                                 tlv_dynamic_descriptor_out_t *out,
                                                 int cmock_num_calls) {
    (void) payload;
    (void) cmock_num_calls;
    out->coin_type = g_coin_type;
    out->magnitude = 6;
    return g_tlv_status;
}

static bool parse_internal_cb(const _internal_tlv_handler_t *handlers,
                              uint8_t handlers_count,
                              tlv_handler_cb_t *common_handler,
                              tag_to_flag_function_t *tag_to_flag_function,
                              const buffer_t *payload,
                              void *tlv_out,
                              TLV_reception_t *received_tags_flags,
                              int cmock_num_calls) {
    (void) common_handler;
    (void) tag_to_flag_function;
    (void) payload;
    (void) received_tags_flags;
    (void) cmock_num_calls;
    // Drive the registered tag handler (handle_tuid_token_address) for coverage.
    if (g_invoke_handler && handlers_count > 0 && handlers[0].func != NULL) {
        tlv_data_t data = {0};
        handlers[0].func(&data, tlv_out);
    }
    return g_tuid_parse_ret;
}

// get_buffer_from_tlv_data is only reached when the handler above runs.
static bool get_buffer_cb(const tlv_data_t *data,
                          buffer_t *out,
                          uint16_t min_size,
                          uint16_t max_size,
                          int cmock_num_calls) {
    (void) data;
    (void) out;
    (void) min_size;
    (void) max_size;
    (void) cmock_num_calls;
    return true;
}

static bool check_tags_cb(TLV_reception_t received,
                          const TLV_tag_t *tags,
                          size_t tag_count,
                          int cmock_num_calls) {
    (void) received;
    (void) tags;
    (void) tag_count;
    (void) cmock_num_calls;
    return g_tags_ok;
}

static int io_send_cb(const buffer_t *r, size_t c, uint16_t sw, int cmock_num_calls) {
    (void) r;
    (void) c;
    (void) cmock_num_calls;
    g_last_sw = sw;
    return (int) sw;
}

void setUp(void) {
    Mocktlv_use_case_dynamic_descriptor_Init();
    Mocktlv_library_Init();
    Mockio_Init();

    tlv_use_case_dynamic_descriptor_Stub(tlv_uc_cb);
    _parse_tlv_internal_Stub(parse_internal_cb);
    tlv_check_received_tags_Stub(check_tags_cb);
    get_buffer_from_tlv_data_Stub(get_buffer_cb);
    io_send_response_buffers_Stub(io_send_cb);

    // Defaults: a fully valid descriptor.
    g_tlv_status = TLV_DYNAMIC_DESCRIPTOR_SUCCESS;
    g_coin_type = BOILERPLATE_SLIP44_COIN_TYPE;
    g_tuid_parse_ret = true;
    g_tags_ok = true;
    g_last_sw = 0;
    g_set_token_called = false;
    g_invoke_handler = false;
}

void tearDown(void) {
    Mocktlv_use_case_dynamic_descriptor_Verify();
    Mocktlv_library_Verify();
    Mockio_Verify();

    Mocktlv_use_case_dynamic_descriptor_Destroy();
    Mocktlv_library_Destroy();
    Mockio_Destroy();
}

static uint8_t g_payload[4] = {0x10, 0x20, 0x00, 0x00};

static int dispatch(void) {
    buffer_t cdata = {.ptr = g_payload, .size = sizeof(g_payload), .offset = 0};
    return handler_provide_token_info(&cdata);
}

void test_tlv_descriptor_failure(void) {
    g_tlv_status = TLV_DYNAMIC_DESCRIPTOR_PARSING_ERROR;
    TEST_ASSERT_EQUAL(SW_INVALID_DYNAMIC_TOKEN, dispatch());
    TEST_ASSERT_FALSE(g_set_token_called);
}

void test_invalid_coin_type(void) {
    g_coin_type = 0x12345678;  // neither 0x8001 nor 0x80008001
    TEST_ASSERT_EQUAL(SW_INVALID_DYNAMIC_TOKEN, dispatch());
    TEST_ASSERT_FALSE(g_set_token_called);
}

void test_tuid_parse_failure(void) {
    g_tuid_parse_ret = false;
    TEST_ASSERT_EQUAL(SW_INVALID_DYNAMIC_TOKEN, dispatch());
    TEST_ASSERT_FALSE(g_set_token_called);
}

void test_missing_token_address_tag(void) {
    g_tags_ok = false;
    TEST_ASSERT_EQUAL(SW_INVALID_DYNAMIC_TOKEN, dispatch());
    TEST_ASSERT_FALSE(g_set_token_called);
}

void test_success_hardened_coin_type(void) {
    g_coin_type = BOILERPLATE_SLIP44_COIN_TYPE_HARDENED;  // also valid
    TEST_ASSERT_EQUAL(SWO_SUCCESS, dispatch());
    TEST_ASSERT_TRUE(g_set_token_called);
}

void test_success(void) {
    TEST_ASSERT_EQUAL(SWO_SUCCESS, dispatch());
    TEST_ASSERT_TRUE(g_set_token_called);
}

// Drives the generated TUID parser into the TAG_TOKEN_ADDRESS handler
// (handle_tuid_token_address). It just forwards to get_buffer_from_tlv_data
// (mocked) -- exercised purely to cover that handler.
void test_tuid_token_address_handler(void) {
    g_invoke_handler = true;
    TEST_ASSERT_EQUAL(SWO_SUCCESS, dispatch());
    TEST_ASSERT_TRUE(g_set_token_called);
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_tlv_descriptor_failure);
    RUN_TEST(test_invalid_coin_type);
    RUN_TEST(test_tuid_parse_failure);
    RUN_TEST(test_missing_token_address_tag);
    RUN_TEST(test_success_hardened_coin_type);
    RUN_TEST(test_success);
    RUN_TEST(test_tuid_token_address_handler);

    return UNITY_END();
}
