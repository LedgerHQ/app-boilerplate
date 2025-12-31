#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <cmocka.h>

#include "handler/sign_tx.h"
#include "buffer.h"
#include "cardano_swo.h"
#include "globals.h"
#include "display.h"
#include "securityPolicy/securityPolicy.h"
#include "transaction/tx_utils.h"
#include "transaction/tx_parse.h"
#include "hexUtils.h"
#include "utils/utils.h"

typedef enum {
    STATUS_TYPE_TRANSACTION_SIGNED = 0,
    STATUS_TYPE_TRANSACTION_REJECTED = 1,
} nbgl_reviewStatusType_t;

#define P1_TX_INIT 0x00
#define P1_TX_DATA_CHUNK 0x01
#define P1_TX_CHUNK_LAST 0x02

// ----------------------------------------------------------------------
// Simple mocks for IO and UI plumbing so we can drive the handler
// ----------------------------------------------------------------------

static uint16_t g_last_sw = 0;

int io_send_response_pointer(const uint8_t *buffer, size_t bufferLength, uint16_t sw) {
    (void) buffer;
    (void) bufferLength;
    g_last_sw = sw;
    return 0;
}

int io_send_sw(uint16_t sw) {
    g_last_sw = sw;
    return 0;
}

void nbgl_useCaseSpinner(const char *text) {
    (void) text;
}

void nbgl_useCaseStatus(const char *text, bool success, void (*callback)(void)) {
    (void) text;
    (void) success;
    if (callback != NULL) {
        callback();
    }
}

void nbgl_useCaseReviewStatus(nbgl_reviewStatusType_t reviewStatusType, void (*callback)(void)) {
    (void) reviewStatusType;
    if (callback != NULL) {
        callback();
    }
}

void ui_menu_main(void) {
    // no-op
}

int ui_display_transaction(void) {
    // reject tests never reach UI confirmation
    tx_review_cleanup();
    return 0;
}

int ui_display_witness(const bip44_path_t *path,
                       security_policy_t policy,
                       warning_bits_t warnings) {
    (void) path;
    (void) policy;
    (void) warnings;
    return 0;
}

bool app_mem_init(void) {
    return true;
}

void app_mem_deinit(void) {
    // no-op
}

void app_mem_dump_stats(void) {
    // no-op
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

// ----------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------

static void reset_context(void) {
    memset(&G_context, 0, sizeof(G_context));
    g_last_sw = 0;
}

typedef struct {
    const char *hex_payload;
    uint8_t p1;
    bool more;
} apdu_segment_t;

typedef struct {
    const char *name;
    const char *init_hex;
    const apdu_segment_t *chunks;
    size_t chunk_count;
    uint16_t expected_sw;
    bool expect_init_failure;
    const char *skip_reason;
} sign_tx_reject_fixture_t;

#include "generated_sign_tx_rejects.h"


// ----------------------------------------------------------------------
// Fixture runner
// ----------------------------------------------------------------------

static void run_sign_tx_reject_fixture(const sign_tx_reject_fixture_t *fixture) {
    reset_context();
    assert_true(app_mem_init());

    uint8_t init_raw[512];
    size_t init_len = hex_to_bytes(fixture->init_hex, init_raw, sizeof(init_raw));
    buffer_t init_buf = {
        .ptr = init_raw,
        .size = init_len,
        .offset = 0,
    };

    g_last_sw = 0;
    int init_rc = handler_sign_tx(&init_buf, P1_TX_INIT, false);

    if (fixture->expect_init_failure) {
        assert_int_equal(init_rc, 0);
        assert_int_equal(g_last_sw, fixture->expected_sw);
        assert_int_equal(G_context.req_type, REQUEST_NONE);
        tx_context_cleanup();
        return;
    }

    assert_int_equal(init_rc, 0);
    assert_int_equal(g_last_sw, SWO_SUCCESS);
    assert_int_equal(G_context.req_type, REQUEST_SIGN_TRANSACTION);
    assert_int_equal(G_context.state.tx_state, TX_STATE_CHUNKS);

    bool failure_seen = false;
    for (size_t i = 0; i < fixture->chunk_count; i++) {
        const apdu_segment_t *segment = &fixture->chunks[i];
        uint8_t chunk_raw[512];
        size_t chunk_len = hex_to_bytes(segment->hex_payload, chunk_raw, sizeof(chunk_raw));
        buffer_t chunk_buf = {
            .ptr = chunk_raw,
            .size = chunk_len,
            .offset = 0,
        };
        g_last_sw = 0;
        int chunk_rc = handler_sign_tx(&chunk_buf, segment->p1, segment->more);
        if (g_last_sw != 0) {
            assert_int_equal(g_last_sw, fixture->expected_sw);
            assert_int_equal(chunk_rc, 0);
            failure_seen = true;
            break;
        }
        assert_int_equal(chunk_rc, SWO_SUCCESS);
    }

    assert_true(failure_seen);
    assert_int_equal(G_context.req_type, REQUEST_NONE);
    tx_context_cleanup();
}

static void test_sign_tx_reject_fixture(void **state) {
    const sign_tx_reject_fixture_t *fixture = (const sign_tx_reject_fixture_t *) *state;
    assert_non_null(fixture);
    if (fixture->skip_reason != NULL) {
        print_message("[SKIP] %s: %s\n", fixture->name, fixture->skip_reason);
        skip();
        return;
    }
    run_sign_tx_reject_fixture(fixture);
}

int main(void) {
    const size_t test_count = ARRAY_LEN(SIGN_TX_REJECT_FIXTURES);
    struct CMUnitTest tests[ARRAY_LEN(SIGN_TX_REJECT_FIXTURES)];
    size_t skipped_count = 0;

    for (size_t i = 0; i < test_count; i++) {
        tests[i] = (struct CMUnitTest) {
            .name = SIGN_TX_REJECT_FIXTURES[i].name,
            .test_func = test_sign_tx_reject_fixture,
            .initial_state = (void *) &SIGN_TX_REJECT_FIXTURES[i],
        };
        if (SIGN_TX_REJECT_FIXTURES[i].skip_reason != NULL) {
            skipped_count++;
        }
    }

    int result = cmocka_run_group_tests(tests, NULL, NULL);

    if (skipped_count > 0) {
        print_message("\n");
        print_message("================================================================================\n");
        print_message("WARNING: %zu/%zu TESTS WERE SKIPPED!\n", skipped_count, test_count);
        print_message("================================================================================\n");
        print_message("\nThese tests are not yet implemented. See skip_reason in test fixtures.\n");
        print_message("Test coverage is incomplete until all skipped tests are enabled.\n");
        print_message("================================================================================\n");
        print_message("\n");
        // Return non-zero to make test harness visible of skipped tests
        return 1;
    }

    return result;
}
