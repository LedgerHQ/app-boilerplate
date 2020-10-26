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

// 1st screen: welcome
UX_STEP_NOCB(ux_menu_flow_1_step, pnn, {&C_boilerplate_logo, "Boilerplate", "is ready"});
// 2nd screen: version of the app
UX_STEP_NOCB(ux_menu_flow_2_step, bn, {"Version", APPVERSION});
// 3rd screen: about
UX_STEP_CB(ux_menu_flow_3_step, pb, ui_submenu_about(), {&C_icon_certificate, "About"});
// 4th screen: quit
UX_STEP_VALID(ux_menu_flow_4_step, pb, os_sched_exit(-1), {&C_icon_dashboard_x, "Quit"});

// FLOW for the main menu
UX_FLOW(ux_menu_flow,
        &ux_menu_flow_1_step,
        &ux_menu_flow_2_step,
        &ux_menu_flow_3_step,
        &ux_menu_flow_4_step,
        FLOW_LOOP);

void ui_menu_main() {
    if (G_ux.stack_count == 0) {
        ux_stack_push();
    }

    ux_flow_init(0, ux_menu_flow, NULL);
}

// 1st screen: about info
UX_STEP_NOCB(ux_submenu_about_1_step, bn, {"Boilerplate App", "(c) 2020 Ledger"});
// 2nd screen: back button
UX_STEP_CB(ux_submenu_about_2_step, pb, ui_menu_main(), {&C_icon_back, "Back"});

// FLOW for the about submenu
UX_FLOW(ux_submenu_about_flow, &ux_submenu_about_1_step, &ux_submenu_about_2_step, FLOW_LOOP);

void ui_submenu_about() {
    ux_flow_init(0, ux_submenu_about_flow, NULL);
}
