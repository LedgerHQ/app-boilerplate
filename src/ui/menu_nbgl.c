
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

void nbgl_fullScreenClear(color_t color, bool refresh)
{
  // Draw full screen
  nbgl_area_t area = {
    .x0 = 0,
    .y0 = 0,
    .width = SCREEN_WIDTH,
    .height = SCREEN_HEIGHT,
    // .bpp = NBGL_BPP_1,
    .backgroundColor = color,
  };
  nbgl_frontDrawRect(&area);
  if (refresh)
  {
    nbgl_frontRefreshArea(&area,FULL_COLOR_REFRESH);
  }
}

void app_quit(void) {
    // exit app here
    os_sched_exit(-1);
}

void ui_menu_main(void) {
    nbgl_useCaseHome(APPNAME, &C_stax_app_boilerplate_64px, NULL, false, ui_menu_about, app_quit);
}

// 'About' menu

static const char* const INFO_TYPES[] = {"Version", "Developer"};
static const char* const INFO_CONTENTS[] = {APPVERSION, "Ledger"};

static bool nav_callback(uint8_t page, nbgl_pageContent_t* content) {
    UNUSED(page);
    content->type = INFOS_LIST;
    content->infosList.nbInfos = 2;
    content->infosList.infoTypes = (const char**) INFO_TYPES;
    content->infosList.infoContents = (const char**) INFO_CONTENTS;
    return true;
}

void ui_menu_about() {
    nbgl_useCaseSettings(APPNAME, 0, 1, false, ui_menu_main, nav_callback, NULL);
}

#endif
