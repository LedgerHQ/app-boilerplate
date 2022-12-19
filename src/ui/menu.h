#pragma once

#include "nbgl_types.h"

/**
 * Show main menu (ready screen, version, about, quit).
 */
void ui_menu_main(void);

/**
 * Show about submenu (copyright, date).
 */
void ui_menu_about(void);

void nbgl_fullScreenClear(color_t color, bool refresh);
