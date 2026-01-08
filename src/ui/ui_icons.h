#pragma once

#include "glyphs.h"

/**
 * Device-specific icon definitions for UI
 */

#if defined(TARGET_NANOX) || defined(TARGET_NANOS2)
#define ICON_APP_CARDANO     C_icon_ada_nanox
#define ICON_APP_HOME        ICON_APP_CARDANO
#define ICON_APP_WARNING     C_icon_warning
#elif defined(TARGET_STAX)
#define ICON_APP_CARDANO     C_icon_ada_stax
#define ICON_APP_HOME        ICON_APP_CARDANO
#define ICON_APP_WARNING     C_Warning_64px
#elif defined(TARGET_FLEX)
#define ICON_APP_CARDANO     C_icon_ada_flex
#define ICON_APP_HOME        ICON_APP_CARDANO
#define ICON_APP_WARNING     C_Warning_64px
#elif defined(TARGET_APEX_P)
#define ICON_APP_CARDANO     C_icon_ada_apex
#define ICON_APP_HOME        ICON_APP_CARDANO
#define ICON_APP_WARNING     LARGE_WARNING_ICON
#endif
