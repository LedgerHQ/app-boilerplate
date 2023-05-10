
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

#define EXCLUDE_BORDER 8

void app_quit(void) {
    // exit app here
    os_sched_exit(-1);
}

enum {
    BLACK_SCREEN,
    DARK_GRAY_SCREEN,
    LIGHT_GRAY_SCREEN,
    WHITE_SCREEN,
    CHECKER_BOARD,
    GATELINES,
    SOURCELINES,
    RANDPIX,
    RANDPIX_INV,
    MAX_SCREENS
} test_screens_e;

void draw_full_screen(color_t color) {
    nbgl_area_t rectArea;
    rectArea.y0 = 0;
    rectArea.x0 = 0;
    rectArea.width = SCREEN_WIDTH;
    rectArea.height = SCREEN_HEIGHT;
    rectArea.backgroundColor = color;
    nbgl_frontDrawRect(&rectArea);
    nbgl_frontRefreshArea(&rectArea, FULL_COLOR_CLEAN_REFRESH, POST_REFRESH_KEEP_POWER_STATE);
}

void draw_black(void) {
    draw_full_screen(BLACK);
}

void draw_dark_gray(void) {
    draw_full_screen(DARK_GRAY);
}

void draw_light_gray(void) {
    draw_full_screen(LIGHT_GRAY);
}

void draw_white(void) {
    draw_full_screen(WHITE);
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
    nbgl_frontRefreshArea(&rectArea, FULL_COLOR_CLEAN_REFRESH, POST_REFRESH_KEEP_POWER_STATE);
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
    nbgl_frontRefreshArea(&rectArea, FULL_COLOR_CLEAN_REFRESH, POST_REFRESH_KEEP_POWER_STATE);
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
    nbgl_frontRefreshArea(&rectArea, FULL_COLOR_CLEAN_REFRESH, POST_REFRESH_KEEP_POWER_STATE);
}

void draw_randpix(color_t background, color_t forground, uint8_t nb_randpix) {
    uint16_t random;
    uint8_t mask;
    // Draw background
    nbgl_area_t backArea;
    nbgl_area_t pixArea;
    backArea.y0 = 0;
    backArea.x0 = 0;
    backArea.width = SCREEN_WIDTH;
    backArea.height = SCREEN_HEIGHT;
    backArea.backgroundColor = background;
    nbgl_frontDrawRect(&backArea);
    #if 0
    mask = 1;
    pixArea.width = 0;
    pixArea.x0 = 0;
    for (uint8_t i=0; i<4; i++) {
        pixArea.y0 = 40;
        pixArea.x0 += 40;
        pixArea.width = 1;
        pixArea.height = 4;
        pixArea.backgroundColor = background;
        nbgl_frontDrawHorizontalLine(&pixArea, mask, forground);
        mask = mask << 1;
    }
    #else
    // Draw random pixels
    for (uint8_t i=0; i<nb_randpix; i++) {
        cx_get_random_bytes(&random, 2);
        pixArea.y0 = random%(SCREEN_HEIGHT-EXCLUDE_BORDER) - (random%(SCREEN_HEIGHT-EXCLUDE_BORDER))%4;
        mask = 1 << (random%(SCREEN_HEIGHT-EXCLUDE_BORDER))%4;
        cx_get_random_bytes(&random, 2);
        pixArea.x0 = random%(SCREEN_WIDTH-EXCLUDE_BORDER);
        pixArea.width = 1;
        pixArea.height = 4;
        pixArea.backgroundColor = background;
        nbgl_frontDrawHorizontalLine(&pixArea, mask, forground);
    }
    #endif
    nbgl_frontRefreshArea(&backArea, FULL_COLOR_CLEAN_REFRESH, POST_REFRESH_KEEP_POWER_STATE);
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
        case DARK_GRAY_SCREEN:
            draw_dark_gray();
            break;
        case LIGHT_GRAY_SCREEN:
            draw_light_gray();
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
        case RANDPIX:
            draw_randpix(WHITE, BLACK, 5);
            break;
        case RANDPIX_INV:
            draw_randpix(BLACK, WHITE, 5);
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
