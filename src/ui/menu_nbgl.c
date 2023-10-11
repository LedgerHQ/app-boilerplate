
/*****************************************************************************
 *   Ledger App Boilerplate.
 *   (c) 2020 Ledger SAS.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *****************************************************************************/

#ifdef HAVE_NBGL

#include "os.h"
#include "glyphs.h"
#include "nbgl_use_case.h"

#include "../globals.h"
#include "menu.h"

//  -----------------------------------------------------------
//  ----------------------- HOME PAGE -------------------------
//  -----------------------------------------------------------

void app_quit(void) {
    // exit app here
    os_sched_exit(-1);
}

// home page definition
void ui_menu_main(void) {
// This parameter shall be set to false if the settings page contains only information
// about the application (version , developer name, ...). It shall be set to
// true if the settings page also contains user configurable parameters related to the
// operation of the application.
#define SETTINGS_BUTTON_ENABLED (true)

    nbgl_useCaseHome(APPNAME,
                     &C_app_boilerplate_64px,
                     NULL,
                     SETTINGS_BUTTON_ENABLED,
                     ui_menu_settings,
                     app_quit);
}

//  -----------------------------------------------------------
//  --------------------- SETTINGS MENU -----------------------
//  -----------------------------------------------------------

static const char* const INFO_TYPES[] = {"Version", "Developer"};
static const char* const INFO_CONTENTS[] = {APPVERSION, "Ledger"};

// settings switches definitions
enum { DUMMY_SWITCH_1_TOKEN = FIRST_USER_TOKEN, DUMMY_SWITCH_2_TOKEN };
enum { DUMMY_SWITCH_1_ID = 0, DUMMY_SWITCH_2_ID, SETTINGS_SWITCHES_NB };

static nbgl_layoutSwitch_t switches[SETTINGS_SWITCHES_NB] = {0};

static bool nav_callback(uint8_t page, nbgl_pageContent_t* content) {
    UNUSED(page);

    // the first settings page contains only the version and the developer name
    // of the app (shall be always on the first setting page)
    if (page == 0) {
        content->type = INFOS_LIST;
        content->infosList.nbInfos = 2;
        content->infosList.infoTypes = INFO_TYPES;
        content->infosList.infoContents = INFO_CONTENTS;
    }
    // the second settings page contains 2 toggle setting switches
    else if (page == 1) {
        switches[DUMMY_SWITCH_1_ID].initState = (nbgl_state_t) N_storage.dummy1_allowed;
        switches[DUMMY_SWITCH_1_ID].text = "Dummy 1";
        switches[DUMMY_SWITCH_1_ID].subText = "Allow dummy 1\nin transactions";
        switches[DUMMY_SWITCH_1_ID].token = DUMMY_SWITCH_1_TOKEN;
        switches[DUMMY_SWITCH_1_ID].tuneId = TUNE_TAP_CASUAL;

        switches[DUMMY_SWITCH_2_ID].initState = (nbgl_state_t) N_storage.dummy2_allowed;
        switches[DUMMY_SWITCH_2_ID].text = "Dummy 2";
        switches[DUMMY_SWITCH_2_ID].subText = "Allow dummy 2\nin transactions";
        switches[DUMMY_SWITCH_2_ID].token = DUMMY_SWITCH_2_TOKEN;
        switches[DUMMY_SWITCH_2_ID].tuneId = TUNE_TAP_CASUAL;

        content->type = SWITCHES_LIST;
        content->switchesList.nbSwitches = SETTINGS_SWITCHES_NB;
        content->switchesList.switches = (nbgl_layoutSwitch_t*) switches;
    } else {
        return false;
    }
    // valid page so return true
    return true;
}

// callback for setting warning choice
static void review_warning_choice(bool confirm) {
    uint8_t switch_value;
    if (confirm) {
        // toggle the switch value
        switch_value = !N_storage.dummy2_allowed;
        // store the new setting value in NVM
        nvm_write((void*) &N_storage.dummy2_allowed, &switch_value, 1);
    }

    // return to the settings menu
    ui_menu_settings();
}

static void controls_callback(int token, uint8_t index) {
    UNUSED(index);
    uint8_t switch_value;
    if (token == DUMMY_SWITCH_1_TOKEN) {
        // Dummy 1 switch touched
        // toggle the switch value
        switch_value = !N_storage.dummy1_allowed;
        // store the new setting value in NVM
        nvm_write((void*) &N_storage.dummy1_allowed, &switch_value, 1);
    } else if (token == DUMMY_SWITCH_2_TOKEN) {
        // Dummy 2 switch touched

        // in this example we display a warning when the user wants
        // to activate the dummy 2 setting
        if (!N_storage.dummy2_allowed) {
            // Display the warning message and ask the user to confirm
            nbgl_useCaseChoice(&C_warning64px,
                               "Dummy 2",
                               "Are you sure to\nallow dummy 2\nin transactions?",
                               "I understand, confirm",
                               "Cancel",
                               review_warning_choice);
        } else {
            // toggle the switch value
            switch_value = !N_storage.dummy2_allowed;
            // store the new setting value in NVM
            nvm_write((void*) &N_storage.dummy2_allowed, &switch_value, 1);
        }
    }
}

// settings menu definition
void ui_menu_settings() {
#define TOTAL_SETTINGS_PAGE  (2)
#define INIT_SETTINGS_PAGE   (0)
#define DISABLE_SUB_SETTINGS (false)
    nbgl_useCaseSettings(APPNAME,
                         INIT_SETTINGS_PAGE,
                         TOTAL_SETTINGS_PAGE,
                         DISABLE_SUB_SETTINGS,
                         ui_menu_main,
                         nav_callback,
                         controls_callback);
}

#endif
