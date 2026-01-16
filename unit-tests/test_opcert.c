#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>

#include <cmocka.h>

#include "globals.h"
#include "securityPolicy/securityPolicy.h"
#include "apdu/dispatcher.h"
#include "handler/sign_opcert.h"
#include "opcert_parse.h"

#include "blake2b.h"

// ----------------------------------------------------------------------
// Simple mocks for IO and UI plumbing so we can drive the handler
// ----------------------------------------------------------------------

typedef enum {
    STATUS_TYPE_OPCERT_SIGNED = 0,
    STATUS_TYPE_OPCERT_REJECTED = 1,
} nbgl_reviewStatusType_t;

// UI display mocks
void ui_menu_main(void) {
    // no-op
}

void ui_display_opcert(security_policy_t policy) {
    (void) policy;
}

void nbgl_useCaseStatus(const char *msg, bool status, void (*cb)(void)) {
    (void) msg;
    (void) status;
    (void) cb;
}

int io_send_response_buffers(const buffer_t *buffer_list, size_t buffer_count, uint16_t sw) {
    (void) buffer_list;
    (void) buffer_count;
    (void) sw;
    return 0;
}

int io_send_sw(uint16_t sw)
{
    return io_send_response_buffers(NULL, 0, sw);
}


int io_send_response_pointer(const uint8_t *buffer, size_t bufferLength, uint16_t sw) {
    (void) buffer;
    (void) bufferLength;
    (void) sw;
    return 0;
}

void *app_mem_alloc_impl(size_t size, bool persistent, const char *file, int line) {
    (void) persistent;
    (void) file;
    (void) line;
    return malloc(size);
}

void app_mem_free_impl(void *ptr, const char *file, int line) {
    (void) file;
    (void) line;
    free(ptr);
}

static void reset_context(void) {
    memset(&G_context, 0, sizeof(G_context));
}

// ----------------------------------------------------------------------
// Test fixture
// ----------------------------------------------------------------------

uint8_t opCert[32 + 8 + 8 + 17] = {
    // kesPublicKey 3d24bc547388cf2403fd978fc3d3a93d1f39acf68a9c00e40512084dc05f2822
    0x3d, 0x24, 0xbc, 0x54, 0x73, 0x88, 0xcf, 0x24, 0x03, 0xfd, 0x97, 0x8f, 0xc3, 0xd3, 0xa9, 0x3d, 
    0x1f, 0x39, 0xac, 0xf6, 0x8a, 0x9c, 0x00, 0xe4, 0x05, 0x12, 0x08, 0x4d, 0xc0, 0x5f, 0x28, 0x22,

    // issueCounter 42
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2a,

    // kesPeriod 47
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2f,

    // Cold Key Path
    0x04,  // Number of components
    0x80,
    0x00,
    0x07,
    0x3D,  // 1853'
    0x80,
    0x00,
    0x07,
    0x17,  // 1815'
    0x80,
    0x00,
    0x00,
    0x00,  // 0'
    0x80,
    0x00,
    0x00,
    0x00  // 0'
};

uint8_t opCertSignature[64] = {
    0x8a, 0x95, 0x0f, 0x72, 0xab, 0x94, 0xe3, 0xb2, 0xac, 0x4d, 0xf1, 0xbb, 0xd8, 0x27, 0x09, 0x82,
    0x7e, 0xc5, 0x72, 0x25, 0x6f, 0xe8, 0x25, 0x7c, 0x82, 0xb3, 0x67, 0xfa, 0xde, 0x03, 0xa2, 0x25,
    0xe6, 0x69, 0x8b, 0xbe, 0x25, 0xb3, 0x8d, 0x35, 0xf8, 0xbf, 0xc3, 0xbf, 0x8f, 0x32, 0x05, 0xf5,
    0x9f, 0x36, 0x14, 0x96, 0x1b, 0x11, 0x57, 0x4e, 0x11, 0x1c, 0xaa, 0x28, 0xc9, 0x76, 0xe9, 0x06
};

// ----------------------------------------------------------------------
// Fixture runner
// ----------------------------------------------------------------------

static void test_handler_sign_opcert(void **state) {
    reset_context();
    (void) state;
    buffer_t cdata = {.ptr = (uint8_t *) opCert, .size = sizeof(opCert), .offset = 0};
    handler_sign_opcert(&cdata);

    assert_int_equal(G_context.req_type, REQUEST_SIGN_OPCERT);
    assert_int_equal(G_context.state.opcert_state, OPCERT_STATE_PARSED);

    const parsed_opcert_t *opcert = &G_context.opcert_info.opcert;
    warning_bits_t warnings = 0;
    warning_bits_init(&warnings);
    security_policy_t policy = policyForSignOpCert(&opcert->poolColdKeyPath, &warnings);
    assert_int_not_equal(policy, POLICY_DENY);
}

static void test_finalize_sign_opcert_aproved(void **state) {
    (void) state;
    finalize_sign_opcert(true);
    assert_int_equal(G_context.state.opcert_state, OPCERT_STATE_NONE);
    assert_int_equal(G_context.req_type, REQUEST_NONE);
    for (size_t i = 0; i < ED25519_SIGNATURE_LENGTH; i++) {
        assert_int_equal(G_context.opcert_info.signature[i], opCertSignature[i]);
    }
}

static void test_finalize_sign_opcert_denial(void **state) {
    (void) state;
    finalize_sign_opcert(false);
    assert_int_equal(G_context.state.opcert_state, OPCERT_STATE_NONE);
    assert_int_equal(G_context.req_type, REQUEST_NONE);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_handler_sign_opcert),
        cmocka_unit_test(test_finalize_sign_opcert_aproved),
        cmocka_unit_test(test_finalize_sign_opcert_denial),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}