
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
#include "nbgl_front.h"

#include "../globals.h"
#include "menu.h"

void app_quit(void) {
    // exit app here
    os_sched_exit(-1);
}

enum {
    BLACK_SCREEN,
    WHITE_SCREEN,
    CHECKER_BOARD,
    GATELINES,
    SOURCELINES,
    MAX_SCREENS
} test_screens_e;

void draw_black(void) {
    nbgl_area_t rectArea;
    rectArea.y0 = 0;
    rectArea.x0 = 0;
    rectArea.width = SCREEN_WIDTH;
    rectArea.height = SCREEN_HEIGHT;
    rectArea.backgroundColor = BLACK;
    nbgl_frontDrawRect(&rectArea);
    nbgl_frontRefreshArea(&rectArea, FULL_COLOR_CLEAN_REFRESH);
}

void draw_white(void) {
    nbgl_area_t rectArea;
    rectArea.y0 = 0;
    rectArea.x0 = 0;
    rectArea.width = SCREEN_WIDTH;
    rectArea.height = SCREEN_HEIGHT;
    rectArea.backgroundColor = WHITE;
    nbgl_frontDrawRect(&rectArea);
    nbgl_frontRefreshArea(&rectArea, FULL_COLOR_CLEAN_REFRESH);
}

void draw_checkerboard(void) {
    nbgl_area_t rectArea;
    rectArea.x0 = 0;
    rectArea.y0 = 0;
    rectArea.width = 1;
    rectArea.height = SCREEN_HEIGHT;
    rectArea.bpp = 0;
    rectArea.backgroundColor = WHITE;

    uint8_t lineBuffer[SCREEN_HEIGHT/8];
    uint8_t a = 0b10101010;
    uint8_t b = 0b01010101;

    while (rectArea.x0 < SCREEN_WIDTH) {
        memset(lineBuffer, rectArea.x0%2?a:b, sizeof(lineBuffer));
        nbgl_frontDrawImage(&rectArea, lineBuffer, NO_TRANSFORMATION, BLACK);
        rectArea.x0 += 1;
    }

    rectArea.x0 = 0;
    rectArea.y0 = 0;
    rectArea.width = SCREEN_WIDTH;
    rectArea.height = SCREEN_HEIGHT;
    nbgl_frontRefreshArea(&rectArea, FULL_COLOR_CLEAN_REFRESH);
}

void draw_gates(void) {
    nbgl_area_t rectArea;
    rectArea.y0 = 0;
    rectArea.x0 = 0;
    rectArea.width = SCREEN_WIDTH;
    rectArea.height = 4;
    rectArea.bpp = 0;
    rectArea.backgroundColor = WHITE;

    while (rectArea.y0 < SCREEN_HEIGHT) {
        nbgl_frontDrawHorizontalLine(&rectArea, 0b1010, WHITE);
        nbgl_frontDrawHorizontalLine(&rectArea, 0b0101, BLACK);
        rectArea.y0 += 4;
    }

    rectArea.x0 = 0;
    rectArea.y0 = 0;
    rectArea.width = SCREEN_WIDTH;
    rectArea.height = SCREEN_HEIGHT;
    nbgl_frontRefreshArea(&rectArea, FULL_COLOR_CLEAN_REFRESH);
}


void draw_sources(void) {
    nbgl_area_t rectArea;

    rectArea.x0 = 0;
    rectArea.y0 = 0;
    rectArea.width = 1;
    rectArea.height = SCREEN_HEIGHT;
    rectArea.bpp = 0;
    while (rectArea.x0 < SCREEN_WIDTH) {
        rectArea.backgroundColor = rectArea.x0 % 2 ? WHITE : BLACK;
        nbgl_frontDrawRect(&rectArea);
        rectArea.x0 += 1;
    }
    rectArea.y0 = 0;
    rectArea.x0 = 0;
    rectArea.width = SCREEN_WIDTH;
    rectArea.height = SCREEN_HEIGHT;
    nbgl_frontRefreshArea(&rectArea, FULL_COLOR_CLEAN_REFRESH);
}



uint8_t current_screen = 0;

void ui_change_screen(void) {
    if (current_screen == MAX_SCREENS) {
        app_exit();
    }

    switch (current_screen) {
        case BLACK_SCREEN:
            draw_black();
            break;
        case WHITE_SCREEN:
            draw_white();
            break;
        case CHECKER_BOARD:
            draw_checkerboard();
            break;
        case GATELINES:
            draw_gates();
            break;
        case SOURCELINES:
            draw_sources();
            break;
    }
    current_screen++;
}


void ui_menu_main(void) {
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
