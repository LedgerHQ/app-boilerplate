/*
 * Unit tests for src/ui/nbgl_display_transaction.c.
 *
 * That file exposes three entry points that all funnel through the static
 * ui_display_transaction_bs_token_choice():
 *   - ui_display_transaction()              -> clear sign  (nbgl_useCaseReview)
 *   - ui_display_blind_signed_transaction() -> blind sign  (nbgl_useCaseReviewBlindSigning)
 *   - ui_display_token_transaction()        -> token sign  (nbgl_useCaseReview, 3 pairs)
 *
 * The NBGL API and format_*() are CMock mocks; io / validation / menu are host
 * stubs. We assert the guard status words, the formatting failure paths, which
 * review use case is started (and with how many tag/value pairs), and the
 * confirm/reject callback.
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

// ---- Symbols referenced by the SUT with no host implementation ----

// The SUT references ICON_APP_BOILERPLATE; display.h maps it to a target
// specific glyph symbol. Defining through the macro yields exactly that symbol.
const nbgl_icon_details_t ICON_APP_BOILERPLATE = {0};

global_ctx_t G_context;

// ---- Controllable stubs for non-mocked dependencies ----

static uint16_t g_last_sw;
static bool g_validate_called;
static bool g_validate_choice;
static int g_menu_main_calls;

int io_send_response_buffers(const buffer_t *rdatalist, size_t count, uint16_t sw) {
    (void) rdatalist;
    (void) count;
    g_last_sw = sw;
    return (int) sw;
}

void validate_transaction(bool choice) {
    g_validate_called = true;
    g_validate_choice = choice;
}

void ui_menu_main(void) {
    g_menu_main_calls++;
}

// ---- Captures filled by CMock AddCallback ----

static nbgl_choiceCallback_t captured_choice_cb;
static const nbgl_contentTagValueList_t *captured_pair_list;
static int review_calls;
static int blind_review_calls;

static void on_review(nbgl_operationType_t operationType,
                      const nbgl_contentTagValueList_t *tagValueList,
                      const nbgl_icon_details_t *icon,
                      const char *reviewTitle,
                      const char *reviewSubTitle,
                      const char *finishTitle,
                      nbgl_choiceCallback_t choiceCallback,
                      int cmock_num_calls) {
    (void) operationType;
    (void) icon;
    (void) reviewTitle;
    (void) reviewSubTitle;
    (void) finishTitle;
    (void) cmock_num_calls;
    captured_pair_list = tagValueList;
    captured_choice_cb = choiceCallback;
    review_calls++;
}

static void on_blind_review(nbgl_operationType_t operationType,
                            const nbgl_contentTagValueList_t *tagValueList,
                            const nbgl_icon_details_t *icon,
                            const char *reviewTitle,
                            const char *reviewSubTitle,
                            const char *finishTitle,
                            const nbgl_tipBox_t *tipBox,
                            nbgl_choiceCallback_t choiceCallback,
                            int cmock_num_calls) {
    (void) operationType;
    (void) icon;
    (void) reviewTitle;
    (void) reviewSubTitle;
    (void) finishTitle;
    (void) tipBox;
    (void) cmock_num_calls;
    captured_pair_list = tagValueList;
    captured_choice_cb = choiceCallback;
    blind_review_calls++;
}

static nbgl_reviewStatusType_t captured_status_type;
static nbgl_callback_t captured_status_quit_cb;

static void on_review_status(nbgl_reviewStatusType_t reviewStatusType,
                             nbgl_callback_t quitCallback,
                             int cmock_num_calls) {
    (void) cmock_num_calls;
    captured_status_type = reviewStatusType;
    captured_status_quit_cb = quitCallback;
}

// Address bytes pointed to by the transaction (content is irrelevant: format_hex
// is mocked, but the field must be a valid pointer).
static uint8_t g_to[ADDRESS_LEN];

void setUp(void) {
    Mocknbgl_use_case_Init();
    Mockformat_Init();

    memset(&G_context, 0, sizeof(G_context));
    G_context.state = STATE_PARSED;
    G_context.req_type = CONFIRM_TRANSACTION;
    G_context.tx_info.transaction.value = 1234;
    G_context.tx_info.transaction.to = g_to;
    G_context.tx_info.token_info.ticker = "TKN";
    G_context.tx_info.token_info.decimals = 2;

    g_last_sw = 0;
    g_validate_called = false;
    g_validate_choice = false;
    g_menu_main_calls = 0;

    captured_choice_cb = NULL;
    captured_pair_list = NULL;
    review_calls = 0;
    blind_review_calls = 0;
    captured_status_type = (nbgl_reviewStatusType_t) 0xFF;
    captured_status_quit_cb = NULL;
}

void tearDown(void) {
    Mocknbgl_use_case_Verify();
    Mockformat_Verify();
    Mocknbgl_use_case_Destroy();
    Mockformat_Destroy();
}

// Queue the two formatting mocks for a successful run.
static void expect_formatting_ok(void) {
    format_fpu64_ExpectAnyArgsAndReturn(true);
    format_hex_ExpectAnyArgsAndReturn(0);
}

// =========================================================================
// Guard clauses (shared logic, exercised through ui_display_transaction)
// =========================================================================

void test_display_transaction_wrong_req_type_conditions_not_satisfied(void) {
    G_context.req_type = CONFIRM_ADDRESS;

    int ret = ui_display_transaction();

    TEST_ASSERT_EQUAL_HEX16(SWO_CONDITIONS_NOT_SATISFIED, g_last_sw);
    TEST_ASSERT_EQUAL(SWO_CONDITIONS_NOT_SATISFIED, ret);
    TEST_ASSERT_EQUAL(STATE_NONE, G_context.state);
}

void test_display_transaction_wrong_state_conditions_not_satisfied(void) {
    G_context.state = STATE_NONE;

    int ret = ui_display_transaction();

    TEST_ASSERT_EQUAL_HEX16(SWO_CONDITIONS_NOT_SATISFIED, g_last_sw);
    TEST_ASSERT_EQUAL(SWO_CONDITIONS_NOT_SATISFIED, ret);
}

// =========================================================================
// Formatting failures
// =========================================================================

void test_display_transaction_amount_format_failure_incorrect_data(void) {
    format_fpu64_ExpectAnyArgsAndReturn(false);

    int ret = ui_display_transaction();

    TEST_ASSERT_EQUAL_HEX16(SWO_INCORRECT_DATA, g_last_sw);
    TEST_ASSERT_EQUAL(SWO_INCORRECT_DATA, ret);
}

void test_display_transaction_address_format_failure_incorrect_data(void) {
    format_fpu64_ExpectAnyArgsAndReturn(true);
    format_hex_ExpectAnyArgsAndReturn(-1);

    int ret = ui_display_transaction();

    TEST_ASSERT_EQUAL_HEX16(SWO_INCORRECT_DATA, g_last_sw);
    TEST_ASSERT_EQUAL(SWO_INCORRECT_DATA, ret);
}

// =========================================================================
// Clear-sign flow
// =========================================================================

void test_display_transaction_success_starts_review_with_two_pairs(void) {
    expect_formatting_ok();
    nbgl_useCaseReview_AddCallback(on_review);
    nbgl_useCaseReview_ExpectAnyArgs();

    int ret = ui_display_transaction();

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(1, review_calls);
    TEST_ASSERT_EQUAL(0, blind_review_calls);
    TEST_ASSERT_NOT_NULL(captured_pair_list);
    TEST_ASSERT_EQUAL(2, captured_pair_list->nbPairs);  // Amount + To
    TEST_ASSERT_NOT_NULL(captured_choice_cb);
}

// =========================================================================
// Blind-sign flow
// =========================================================================

void test_display_blind_signed_transaction_starts_blind_review(void) {
    expect_formatting_ok();
    nbgl_useCaseReviewBlindSigning_AddCallback(on_blind_review);
    nbgl_useCaseReviewBlindSigning_ExpectAnyArgs();

    int ret = ui_display_blind_signed_transaction();

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(1, blind_review_calls);
    TEST_ASSERT_EQUAL(0, review_calls);
    TEST_ASSERT_EQUAL(2, captured_pair_list->nbPairs);  // Amount + To
}

// =========================================================================
// Token flow
// =========================================================================

void test_display_token_transaction_wrong_req_type_conditions_not_satisfied(void) {
    // Token flow expects CONFIRM_TOKEN_TRANSACTION; default is CONFIRM_TRANSACTION.
    int ret = ui_display_token_transaction();

    TEST_ASSERT_EQUAL_HEX16(SWO_CONDITIONS_NOT_SATISFIED, g_last_sw);
    TEST_ASSERT_EQUAL(SWO_CONDITIONS_NOT_SATISFIED, ret);
}

void test_display_token_transaction_success_starts_review_with_three_pairs(void) {
    G_context.req_type = CONFIRM_TOKEN_TRANSACTION;
    expect_formatting_ok();
    nbgl_useCaseReview_AddCallback(on_review);
    nbgl_useCaseReview_ExpectAnyArgs();

    int ret = ui_display_token_transaction();

    TEST_ASSERT_EQUAL(0, ret);
    TEST_ASSERT_EQUAL(1, review_calls);
    TEST_ASSERT_EQUAL(3, captured_pair_list->nbPairs);  // Token + Amount + To
}

// =========================================================================
// review_choice callback
// =========================================================================

static nbgl_choiceCallback_t get_review_choice_cb(void) {
    expect_formatting_ok();
    nbgl_useCaseReview_AddCallback(on_review);
    nbgl_useCaseReview_ExpectAnyArgs();
    ui_display_transaction();
    return captured_choice_cb;
}

void test_review_choice_confirm_validates_and_shows_signed(void) {
    nbgl_choiceCallback_t cb = get_review_choice_cb();

    nbgl_useCaseReviewStatus_AddCallback(on_review_status);
    nbgl_useCaseReviewStatus_ExpectAnyArgs();
    cb(true);

    TEST_ASSERT_TRUE(g_validate_called);
    TEST_ASSERT_TRUE(g_validate_choice);
    TEST_ASSERT_EQUAL(STATUS_TYPE_TRANSACTION_SIGNED, captured_status_type);
    TEST_ASSERT_EQUAL_PTR(ui_menu_main, captured_status_quit_cb);
}

void test_review_choice_reject_validates_and_shows_rejected(void) {
    nbgl_choiceCallback_t cb = get_review_choice_cb();

    nbgl_useCaseReviewStatus_AddCallback(on_review_status);
    nbgl_useCaseReviewStatus_ExpectAnyArgs();
    cb(false);

    TEST_ASSERT_TRUE(g_validate_called);
    TEST_ASSERT_FALSE(g_validate_choice);
    TEST_ASSERT_EQUAL(STATUS_TYPE_TRANSACTION_REJECTED, captured_status_type);
    TEST_ASSERT_EQUAL_PTR(ui_menu_main, captured_status_quit_cb);
}

// =========================================================================

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_display_transaction_wrong_req_type_conditions_not_satisfied);
    RUN_TEST(test_display_transaction_wrong_state_conditions_not_satisfied);
    RUN_TEST(test_display_transaction_amount_format_failure_incorrect_data);
    RUN_TEST(test_display_transaction_address_format_failure_incorrect_data);
    RUN_TEST(test_display_transaction_success_starts_review_with_two_pairs);
    RUN_TEST(test_display_blind_signed_transaction_starts_blind_review);
    RUN_TEST(test_display_token_transaction_wrong_req_type_conditions_not_satisfied);
    RUN_TEST(test_display_token_transaction_success_starts_review_with_three_pairs);
    RUN_TEST(test_review_choice_confirm_validates_and_shows_signed);
    RUN_TEST(test_review_choice_reject_validates_and_shows_rejected);

    return UNITY_END();
}
