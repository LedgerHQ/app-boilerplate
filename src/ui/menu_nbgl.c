
/*****************************************************************************
 *   Ledger App Cardano.
 *   (c) 2025 Vacuumlabs
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

#include "os.h"
#include "glyphs.h"
#include "nbgl_use_case.h"

#include "globals.h"
#include "menu.h"
#include "display.h"
#include "settings.h"

//  -----------------------------------------------------------
//  ----------------------- HOME PAGE -------------------------
//  -----------------------------------------------------------

void app_quit(void) {
    // exit app here
    os_sched_exit(-1);
}

//  -----------------------------------------------------------
//  --------------------- SETTINGS MENU -----------------------
//  -----------------------------------------------------------
#define SETTING_INFO_NB 2
static const char* const INFO_TYPES[SETTING_INFO_NB] = {"Version", "Developer"};
static const char* const INFO_CONTENTS[SETTING_INFO_NB] = {APPVERSION, "Vacuumlabs"};

// settings switches definitions
enum { EXPERT_MODE_TOKEN = FIRST_USER_TOKEN, SILENT_PUBKEY_EXPORT_TOKEN };
enum { EXPERT_MODE_ID = 0, SILENT_PUBKEY_EXPORT_ID, SETTINGS_SWITCHES_NB };

static nbgl_contentSwitch_t switches[SETTINGS_SWITCHES_NB] = {0};

static const nbgl_contentInfoList_t infoList = {
    .nbInfos = SETTING_INFO_NB,
    .infoTypes = INFO_TYPES,
    .infoContents = INFO_CONTENTS,
};

static uint8_t initSettingPage;
static void controls_callback(int token, uint8_t index, int page);

// settings menu definition
#define SETTING_CONTENTS_NB 1
static const nbgl_content_t contents[SETTING_CONTENTS_NB] = {
    {.type = SWITCHES_LIST,
     .content.switchesList.nbSwitches = SETTINGS_SWITCHES_NB,
     .content.switchesList.switches = switches,
     .contentActionCallback = controls_callback}};

static const nbgl_genericContents_t settingContents = {.callbackCallNeeded = false,
                                                       .contentsList = contents,
                                                       .nbContents = SETTING_CONTENTS_NB};

static void controls_callback(int token, uint8_t index, int page) {
    UNUSED(index);

    initSettingPage = page;

    uint8_t switch_value;
    if (token == EXPERT_MODE_TOKEN) {
        // toggle the switch value
        switch_value = flip_bool_setting(N_storage.expert_mode_enabled);
        switches[EXPERT_MODE_ID].initState = (nbgl_state_t) switch_value;
        // store the new setting value in NVM
        nvm_write((void*) &N_storage.expert_mode_enabled, &switch_value, 1);
    } else if (token == SILENT_PUBKEY_EXPORT_TOKEN) {
        // toggle the switch value
        switch_value = flip_bool_setting(N_storage.silent_pubkey_export_enabled);
        switches[SILENT_PUBKEY_EXPORT_ID].initState = (nbgl_state_t) switch_value;
        // store the new setting value in NVM
        nvm_write((void*) &N_storage.silent_pubkey_export_enabled, &switch_value, 1);
    } else {
        // TODO
        ASSERT(false);
    }
}

// home page definition
void ui_menu_main(void) {
    // Initialize switches data
    switches[EXPERT_MODE_ID].initState = (nbgl_state_t) N_storage.expert_mode_enabled;
    switches[EXPERT_MODE_ID].text = "Expert mode";
    switches[EXPERT_MODE_ID].subText = "Show expert details\nin transactions";
    switches[EXPERT_MODE_ID].token = EXPERT_MODE_TOKEN;
#ifdef HAVE_PIEZO_SOUND
    switches[EXPERT_MODE_ID].tuneId = TUNE_TAP_CASUAL;
#endif

    switches[SILENT_PUBKEY_EXPORT_ID].initState = (nbgl_state_t) N_storage.silent_pubkey_export_enabled;
    switches[SILENT_PUBKEY_EXPORT_ID].text = "Silent public key export";
    switches[SILENT_PUBKEY_EXPORT_ID].subText = "Allow usual public keys\nto be exported silently";
    switches[SILENT_PUBKEY_EXPORT_ID].token = SILENT_PUBKEY_EXPORT_TOKEN;
#ifdef HAVE_PIEZO_SOUND
    switches[SILENT_PUBKEY_EXPORT_ID].tuneId = TUNE_TAP_CASUAL;
#endif

    nbgl_useCaseHomeAndSettings(APPNAME,
                                &ICON_APP_HOME,
                                NULL,
                                INIT_HOME_PAGE,
                                &settingContents,
                                &infoList,
                                NULL,
                                app_quit);
}
