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

#include "os.h"
#include "ux.h"
#include "glyphs.h"

#include "../globals.h"
#include "menu.h"


UX_STEP_NOCB(ux_menu_ready_step, pnn, {&C_boilerplate_logo, "Conflux", "is ready"});
UX_STEP_VALID(ux_menu_settings_step, pb, ui_menu_settings(), {&C_icon_coggle, "Settings"});
UX_STEP_NOCB(ux_menu_version_step, bn, {"Version", APPVERSION});
UX_STEP_CB(ux_menu_about_step, pb, ui_menu_about(), {&C_icon_certificate, "About"});
UX_STEP_VALID(ux_menu_exit_step, pb, os_sched_exit(-1), {&C_icon_dashboard_x, "Quit"});

// FLOW for the main menu:
// #1 screen: ready
// #2 screen: settings submenu
// #3 screen: version of the app
// #4 screen: about submenu
// #5 screen: quit
UX_FLOW(ux_menu_main_flow,
        &ux_menu_ready_step,
        &ux_menu_settings_step,
        &ux_menu_version_step,
        &ux_menu_about_step,
        &ux_menu_exit_step,
        FLOW_LOOP);

void ui_menu_main() {
    if (G_ux.stack_count == 0) {
        ux_stack_push();
    }

    ux_flow_init(0, ux_menu_main_flow, NULL);
}

UX_STEP_NOCB(ux_menu_info_step, bn, {"Conflux App", "(c) 2021 Conflux"});
UX_STEP_CB(ux_menu_back_step, pb, ui_menu_main(), {&C_icon_back, "Back"});

// FLOW for the about submenu:
// #1 screen: app info
// #2 screen: back button to main menu
UX_FLOW(ux_menu_about_flow, &ux_menu_info_step, &ux_menu_back_step, FLOW_LOOP);

void ui_menu_about() {
    ux_flow_init(0, ux_menu_about_flow, NULL);
}


// ------------------------------------------ //
// ---------------- SETTINGS ---------------- //
// ------------------------------------------ //

const char* const settings_submenu_getter_values[] = {
  "Allow blind sign",
  "Back",
};

const char* settings_submenu_getter(unsigned int idx) {
  if (idx < ARRAYLEN(settings_submenu_getter_values)) {
    return settings_submenu_getter_values[idx];
  }
  return NULL;
}

const char* const no_yes_data_getter_values[] = {
  "No",
  "Yes",
  "Back"
};

static const char* allow_blind_sign_data_getter(unsigned int idx) {
  if (idx < ARRAYLEN(no_yes_data_getter_values)) {
    return no_yes_data_getter_values[idx];
  }
  return NULL;
}

static void allow_blind_sign_data_change(enum BlindSign blind_sign) {
    uint8_t value = (uint8_t) blind_sign;
    nvm_write((void *)&N_storage.settings.allow_blind_sign, &value, sizeof(value));
    ui_menu_main();
}

void allow_blind_sign_data_selector(unsigned int idx) {
  switch(idx) {
    case 0:
      allow_blind_sign_data_change(BlindSignDisabled);
      break;
    case 1:
      allow_blind_sign_data_change(BlindSignEnabled);
      break;
    default:
      ui_menu_main();
  }
}

void settings_submenu_selector(unsigned int idx) {
  switch(idx) {
    case 0:
      ux_menulist_init_select(0, allow_blind_sign_data_getter, allow_blind_sign_data_selector, N_storage.settings.allow_blind_sign);
      break;
    default:
      ui_menu_main();
  }
}

void ui_menu_settings() {
    ux_menulist_init(0, settings_submenu_getter, settings_submenu_selector);
}
