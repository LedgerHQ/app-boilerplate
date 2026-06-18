/*
 * Unit tests for src/ui/nbgl_display_address.c.
 *
 * nbgl_display_address.c builds the address string from the current public-key
 * context and hands it to the NBGL "address review" use case. We replace the
 * NBGL API and format_hex() with CMock-generated mocks, and provide host stubs
 * for the remaining app/SDK dependencies (io, address derivation, validation,
 * menu). This lets us assert the control flow — early-out status words, the
 * review call, and the confirm/reject callback — without a device.
 *
 * The static review_choice() callback is exercised by capturing the function
 * pointer passed to nbgl_useCaseAddressReview via CMock AddCallback.
 */

#include <string.h>

#include "unity.h"

#include "Mocknbgl_use_case.h"
#include "Mockformat.h"

#include "globals.h"
#include "sw.h"
#include "menu.h"
#include "display.h"
#include "validate.h"
#include "address.h"

// ---- Symbols referenced by nbgl_display_address.c with no host impl ----

// nbgl_display_address.c references ICON_APP_BOILERPLATE; display.h resolves
// that macro to a target-specific glyph symbol (e.g. C_app_boilerplate_64px on
// Flex/Stax). Defining through the macro yields exactly the symbol the SUT
// links against, whatever the active target.
const nbgl_icon_details_t ICON_APP_BOILERPLATE = {0};

// Global request context (declared extern in globals.h).
global_ctx_t G_context;

// ---- Controllable stubs for non-mocked dependencies ----

static uint16_t g_last_sw;          // last status word passed to io_send_sw
static bool g_address_ok;           // address_from_pubkey return value
static bool g_validate_called;      // validate_pubkey was invoked
static bool g_validate_choice;      // argument passed to validate_pubkey
static int g_menu_main_calls;       // ui_menu_main invocations

// io_send_sw() is a static inline in io.h that forwards to this WEAK symbol.
int io_send_response_buffers(const buffer_t *rdatalist, size_t count, uint16_t sw) {
    (void) rdatalist;
    (void) count;
    g_last_sw = sw;
    return (int) sw;
}

bool address_from_pubkey(const uint8_t public_key[static 65], uint8_t *out, size_t out_len) {
    (void) public_key;
    if (g_address_ok) {
        memset(out, 0xAB, out_len);
    }
    return g_address_ok;
}

void validate_pubkey(bool choice) {
    g_validate_called = true;
    g_validate_choice = choice;
}

void ui_menu_main(void) {
    g_menu_main_calls++;
}

// ---- Capture of the choice callback handed to the address review ----

static nbgl_choiceCallback_t captured_choice_cb;

static void on_address_review(const char *address,
                              const nbgl_contentTagValueList_t *additionalTagValueList,
                              const nbgl_icon_details_t *icon,
                              const char *reviewTitle,
                              const char *reviewSubTitle,
                              nbgl_choiceCallback_t choiceCallback,
                              int cmock_num_calls) {
    (void) address;
    (void) additionalTagValueList;
    (void) icon;
    (void) reviewTitle;
    (void) reviewSubTitle;
    (void) cmock_num_calls;
    captured_choice_cb = choiceCallback;
}

// ---- Capture of the review-status call (confirm/reject path) ----

static nbgl_reviewStatusType_t captured_status_type;
static nbgl_callback_t captured_status_quit_cb;

static void on_review_status(nbgl_reviewStatusType_t reviewStatusType,
                             nbgl_callback_t quitCallback,
                             int cmock_num_calls) {
    (void) cmock_num_calls;
    captured_status_type = reviewStatusType;
    captured_status_quit_cb = quitCallback;
}

void setUp(void) {
    Mocknbgl_use_case_Init();
    Mockformat_Init();

    memset(&G_context, 0, sizeof(G_context));
    // Default: valid state for displaying an address.
    G_context.req_type = CONFIRM_ADDRESS;
    G_context.state = STATE_NONE;

    g_last_sw = 0;
    g_address_ok = true;
    g_validate_called = false;
    g_validate_choice = false;
    g_menu_main_calls = 0;

    captured_choice_cb = NULL;
    captured_status_type = (nbgl_reviewStatusType_t) 0xFF;
    captured_status_quit_cb = NULL;
}

void tearDown(void) {
    Mocknbgl_use_case_Verify();
    Mockformat_Verify();
    Mocknbgl_use_case_Destroy();
    Mockformat_Destroy();
}

// =========================================================================
// ui_display_address — guard clauses
// =========================================================================

void test_display_address_wrong_req_type_conditions_not_satisfied(void) {
    G_context.req_type = CONFIRM_TRANSACTION;

    // No NBGL / format mocks expected on this path.
    int ret = ui_display_address();

    TEST_ASSERT_EQUAL_HEX16(SWO_CONDITIONS_NOT_SATISFIED, g_last_sw);
    TEST_ASSERT_EQUAL(SWO_CONDITIONS_NOT_SATISFIED, ret);
    TEST_ASSERT_EQUAL(STATE_NONE, G_context.state);
}

void test_display_address_wrong_state_conditions_not_satisfied(void) {
    G_context.state = STATE_APPROVED;

    int ret = ui_display_address();

    TEST_ASSERT_EQUAL_HEX16(SWO_CONDITIONS_NOT_SATISFIED, g_last_sw);
    TEST_ASSERT_EQUAL(SWO_CONDITIONS_NOT_SATISFIED, ret);
    // The guard resets the state to STATE_NONE before answering.
    TEST_ASSERT_EQUAL(STATE_NONE, G_context.state);
}

// =========================================================================
// ui_display_address — derivation / formatting failures
// =========================================================================

void test_display_address_pubkey_failure_incorrect_data(void) {
    g_address_ok = false;

    // format_hex must not be reached when derivation fails.
    int ret = ui_display_address();

    TEST_ASSERT_EQUAL_HEX16(SWO_INCORRECT_DATA, g_last_sw);
    TEST_ASSERT_EQUAL(SWO_INCORRECT_DATA, ret);
}

void test_display_address_format_failure_incorrect_data(void) {
    format_hex_ExpectAnyArgsAndReturn(-1);

    int ret = ui_display_address();

    TEST_ASSERT_EQUAL_HEX16(SWO_INCORRECT_DATA, g_last_sw);
    TEST_ASSERT_EQUAL(SWO_INCORRECT_DATA, ret);
}

// =========================================================================
// ui_display_address — happy path
// =========================================================================

void test_display_address_success_starts_address_review(void) {
    format_hex_ExpectAnyArgsAndReturn(40);
    nbgl_useCaseAddressReview_AddCallback(on_address_review);
    nbgl_useCaseAddressReview_ExpectAnyArgs();

    int ret = ui_display_address();

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_NOT_NULL(captured_choice_cb);
}

// =========================================================================
// review_choice — confirm / reject
// =========================================================================

static nbgl_choiceCallback_t get_review_choice_cb(void) {
    format_hex_ExpectAnyArgsAndReturn(40);
    nbgl_useCaseAddressReview_AddCallback(on_address_review);
    nbgl_useCaseAddressReview_ExpectAnyArgs();
    ui_display_address();
    return captured_choice_cb;
}

void test_review_choice_confirm_validates_and_shows_verified(void) {
    nbgl_choiceCallback_t cb = get_review_choice_cb();

    nbgl_useCaseReviewStatus_AddCallback(on_review_status);
    nbgl_useCaseReviewStatus_ExpectAnyArgs();
    cb(true);

    TEST_ASSERT_TRUE(g_validate_called);
    TEST_ASSERT_TRUE(g_validate_choice);
    TEST_ASSERT_EQUAL(STATUS_TYPE_ADDRESS_VERIFIED, captured_status_type);
    TEST_ASSERT_EQUAL_PTR(ui_menu_main, captured_status_quit_cb);
}

void test_review_choice_reject_validates_and_shows_rejected(void) {
    nbgl_choiceCallback_t cb = get_review_choice_cb();

    nbgl_useCaseReviewStatus_AddCallback(on_review_status);
    nbgl_useCaseReviewStatus_ExpectAnyArgs();
    cb(false);

    TEST_ASSERT_TRUE(g_validate_called);
    TEST_ASSERT_FALSE(g_validate_choice);
    TEST_ASSERT_EQUAL(STATUS_TYPE_ADDRESS_REJECTED, captured_status_type);
    TEST_ASSERT_EQUAL_PTR(ui_menu_main, captured_status_quit_cb);
}

// =========================================================================

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_display_address_wrong_req_type_conditions_not_satisfied);
    RUN_TEST(test_display_address_wrong_state_conditions_not_satisfied);
    RUN_TEST(test_display_address_pubkey_failure_incorrect_data);
    RUN_TEST(test_display_address_format_failure_incorrect_data);
    RUN_TEST(test_display_address_success_starts_address_review);
    RUN_TEST(test_review_choice_confirm_validates_and_shows_verified);
    RUN_TEST(test_review_choice_reject_validates_and_shows_rejected);

    return UNITY_END();
}
