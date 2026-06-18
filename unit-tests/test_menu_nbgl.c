/*
 * Unit tests for src/ui/menu_nbgl.c.
 *
 * menu_nbgl.c drives the NBGL "use case" API; we replace that API (and the few
 * OS syscalls it uses) with CMock-generated mocks, so we can assert which UI
 * calls are issued without a real device or display stack.
 *
 * Static callbacks (controls_callback, review_warning_choice) are exercised by
 * capturing the function pointer passed to nbgl_useCaseHomeAndSettings /
 * nbgl_useCaseChoice via CMock AddCallback.
 */

#include <string.h>

#include "unity.h"

#include "Mocknbgl_use_case.h"
#include "Mockos_nvm.h"
#include "Mockos_task.h"

#include "globals.h"
#include "menu.h"

// app_quit() is exported by menu_nbgl.c but not declared in a public header.
void app_quit(void);

// Glyph symbols referenced by menu_nbgl.c (through display.h). Which concrete
// symbol the UI uses depends on the active device target, so mirror the same
// selection as src/ui/display.h here.
#if defined(TARGET_STAX) || defined(TARGET_FLEX)
const nbgl_icon_details_t C_app_boilerplate_64px = {0};
const nbgl_icon_details_t C_Warning_64px = {0};
#elif defined(TARGET_APEX_P)
const nbgl_icon_details_t C_app_boilerplate_48px = {0};
#else
const nbgl_icon_details_t C_app_boilerplate_14px = {0};
const nbgl_icon_details_t C_home_boilerplate_14px = {0};
const nbgl_icon_details_t C_icon_warning = {0};
#endif

// On device, N_storage_real lives in NVM (const). In test builds, globals.h
// drops the const qualifier so we can mutate it freely.
internal_storage_t N_storage_real;

// ---- Callback captures (filled by CMock AddCallback) ----

static nbgl_callback_t captured_quit_cb;
static const nbgl_genericContents_t *captured_settings;
static nbgl_choiceCallback_t captured_choice_cb;

static void on_home_and_settings(const char *appName,
                                 const nbgl_icon_details_t *appIcon,
                                 const char *tagline,
                                 const uint8_t initSettingPage,
                                 const nbgl_genericContents_t *settingContents,
                                 const nbgl_contentInfoList_t *infosList,
                                 const nbgl_homeAction_t *action,
                                 nbgl_callback_t quitCallback,
                                 int cmock_num_calls) {
    (void) appName;
    (void) appIcon;
    (void) tagline;
    (void) initSettingPage;
    (void) infosList;
    (void) action;
    (void) cmock_num_calls;
    captured_quit_cb = quitCallback;
    captured_settings = settingContents;
}

static void on_choice(const nbgl_icon_details_t *icon,
                      const char *message,
                      const char *subMessage,
                      const char *confirmText,
                      const char *rejectString,
                      nbgl_choiceCallback_t callback,
                      int cmock_num_calls) {
    (void) icon;
    (void) message;
    (void) subMessage;
    (void) confirmText;
    (void) rejectString;
    (void) cmock_num_calls;
    captured_choice_cb = callback;
}

// Simulates NVM write on host (just a memcpy).
static void on_nvm_write(void *dst, void *src, unsigned int len, int cmock_num_calls) {
    (void) cmock_num_calls;
    memcpy(dst, src, len);
}

void setUp(void) {
    Mocknbgl_use_case_Init();
    Mockos_nvm_Init();
    Mockos_task_Init();

    captured_quit_cb = NULL;
    captured_settings = NULL;
    captured_choice_cb = NULL;

    memset(&N_storage_real, 0, sizeof(N_storage_real));
    N_storage_real.initialized = 1;
}

void tearDown(void) {
    Mocknbgl_use_case_Verify();
    Mockos_nvm_Verify();
    Mockos_task_Verify();
    Mocknbgl_use_case_Destroy();
    Mockos_nvm_Destroy();
    Mockos_task_Destroy();
}

// =========================================================================
// app_quit
// =========================================================================

void test_app_quit_calls_os_sched_exit(void) {
    os_sched_exit_ExpectAnyArgs();
    app_quit();
}

// =========================================================================
// ui_menu_main
// =========================================================================

void test_ui_menu_main_calls_home_and_settings(void) {
    nbgl_useCaseHomeAndSettings_AddCallback(on_home_and_settings);
    nbgl_useCaseHomeAndSettings_ExpectAnyArgs();

    ui_menu_main();

    TEST_ASSERT_NOT_NULL(captured_quit_cb);
    TEST_ASSERT_NOT_NULL(captured_settings);
}

void test_ui_menu_main_quit_callback_exits(void) {
    nbgl_useCaseHomeAndSettings_AddCallback(on_home_and_settings);
    nbgl_useCaseHomeAndSettings_ExpectAnyArgs();
    ui_menu_main();

    os_sched_exit_ExpectAnyArgs();
    captured_quit_cb();
}

// =========================================================================
// controls_callback — dummy 1
// =========================================================================

static nbgl_contentActionCallback_t get_controls_cb(void) {
    nbgl_useCaseHomeAndSettings_AddCallback(on_home_and_settings);
    nbgl_useCaseHomeAndSettings_ExpectAnyArgs();
    ui_menu_main();
    return captured_settings->contentsList[0].contentActionCallback;
}

void test_toggle_dummy1_writes_nvm(void) {
    nbgl_contentActionCallback_t cb = get_controls_cb();

    nvm_write_AddCallback(on_nvm_write);
    nvm_write_ExpectAnyArgs();
    cb(FIRST_USER_TOKEN, 0, 0);

    TEST_ASSERT_EQUAL(1, N_storage_real.dummy1_allowed);
}

void test_toggle_dummy1_twice_restores(void) {
    nbgl_contentActionCallback_t cb = get_controls_cb();

    nvm_write_AddCallback(on_nvm_write);
    nvm_write_ExpectAnyArgs();
    cb(FIRST_USER_TOKEN, 0, 0);
    TEST_ASSERT_EQUAL(1, N_storage_real.dummy1_allowed);

    nvm_write_ExpectAnyArgs();
    cb = get_controls_cb();
    cb(FIRST_USER_TOKEN, 0, 0);
    TEST_ASSERT_EQUAL(0, N_storage_real.dummy1_allowed);
}

// =========================================================================
// controls_callback — dummy 2 (with warning)
// =========================================================================

void test_toggle_dummy2_disabled_shows_warning(void) {
    nbgl_contentActionCallback_t cb = get_controls_cb();

    nbgl_useCaseChoice_AddCallback(on_choice);
    nbgl_useCaseChoice_ExpectAnyArgs();
    cb(FIRST_USER_TOKEN + 1, 0, 0);

    TEST_ASSERT_NOT_NULL(captured_choice_cb);
}

void test_toggle_dummy2_enabled_writes_directly(void) {
    N_storage_real.dummy2_allowed = 1;
    nbgl_contentActionCallback_t cb = get_controls_cb();

    nvm_write_AddCallback(on_nvm_write);
    nvm_write_ExpectAnyArgs();
    cb(FIRST_USER_TOKEN + 1, 0, 0);

    TEST_ASSERT_EQUAL(0, N_storage_real.dummy2_allowed);
}

// =========================================================================
// review_warning_choice
// =========================================================================

void test_warning_confirm_enables_dummy2(void) {
    nbgl_contentActionCallback_t cb = get_controls_cb();

    nbgl_useCaseChoice_AddCallback(on_choice);
    nbgl_useCaseChoice_ExpectAnyArgs();
    cb(FIRST_USER_TOKEN + 1, 0, 2);

    nvm_write_AddCallback(on_nvm_write);
    nvm_write_ExpectAnyArgs();
    nbgl_useCaseHomeAndSettings_ExpectAnyArgs();
    captured_choice_cb(true);

    TEST_ASSERT_EQUAL(1, N_storage_real.dummy2_allowed);
}

void test_warning_cancel_keeps_dummy2_disabled(void) {
    nbgl_contentActionCallback_t cb = get_controls_cb();

    nbgl_useCaseChoice_AddCallback(on_choice);
    nbgl_useCaseChoice_ExpectAnyArgs();
    cb(FIRST_USER_TOKEN + 1, 0, 0);

    nbgl_useCaseHomeAndSettings_ExpectAnyArgs();
    captured_choice_cb(false);

    TEST_ASSERT_EQUAL(0, N_storage_real.dummy2_allowed);
}

// =========================================================================
// Edge cases
// =========================================================================

void test_unknown_token_does_nothing(void) {
    nbgl_contentActionCallback_t cb = get_controls_cb();
    // No Expect set → CMock will fail if any mock is called unexpectedly.
    cb(0xFF, 0, 0);
}

// =========================================================================

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_app_quit_calls_os_sched_exit);
    RUN_TEST(test_ui_menu_main_calls_home_and_settings);
    RUN_TEST(test_ui_menu_main_quit_callback_exits);
    RUN_TEST(test_toggle_dummy1_writes_nvm);
    RUN_TEST(test_toggle_dummy1_twice_restores);
    RUN_TEST(test_toggle_dummy2_disabled_shows_warning);
    RUN_TEST(test_toggle_dummy2_enabled_writes_directly);
    RUN_TEST(test_warning_confirm_enables_dummy2);
    RUN_TEST(test_warning_cancel_keeps_dummy2_disabled);
    RUN_TEST(test_unknown_token_does_nothing);

    return UNITY_END();
}
