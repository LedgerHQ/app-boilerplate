#pragma once

#ifdef SCREEN_SIZE_NANO
#define LARGE_ICON         C_app_nbgl_tests_16px
#define LARGE_WARNING_ICON C_icon_warning
#else
#define LARGE_ICON         C_app_nbgl_tests_64px
#define LARGE_WARNING_ICON C_Warning_64px
#endif

/**
 * Show main menu (ready screen, version, about, quit).
 */
void ui_menu_main(void);

/**
 * Show about submenu (copyright, date).
 */
void ui_menu_about(void);
