/**
 * @file nbgl_step.c
 * @brief Implementation of predefined pages management for Applications
 */

#ifdef NBGL_STEP
/*********************
 *      INCLUDES
 *********************/
#include <string.h>
#include <stdlib.h>

#include "glyphs.h"
#include "os_pic.h"
#include "lib_ux/include/ux.h"
#include "nbgl_step.h"
#include "nbgl_layout.h"


/*********************
 *      DEFINES
 *********************/
// string to store the title for a multi-pages text+subtext
#define TMP_STRING_MAX_LEN 24

///< Maximum number of layers for steps, cannot be greater than max number of layout layers
#ifdef TARGET_NANOS
#undef NB_MAX_LINES
#define NB_MAX_LINES 2
#define NB_MAX_LAYERS 1
#else
#define NB_MAX_LAYERS 3
#endif

static nbgl_layoutButtonCallback_t ctx_callback;
static nbgl_screenTickerConfiguration_t ctx_ticker;
static bool active;
static bool displayed;


void nbgl_process_ux_displayed_event(void)
{
    displayed = true;
    PRINTF("Displayed\n");
}

static void wait_displayed(void)
{
    displayed = false;
    while (!displayed) {
        // - Looping on os_io_seph_recv_and_process(0);
        // - This will send a general_status and then wait for an event.
        // - Upon event reception this will call io_seproxyhal_handle_event()
        //   - On some case this will call io_event() which usually forward the
        //     event to the UX lib.
        io_seproxyhal_spi_recv(G_io_seproxyhal_spi_buffer, sizeof(G_io_seproxyhal_spi_buffer), 0); \
        io_seproxyhal_handle_event();
    }
}

static void display_clear(void) {
    bagl_element_t element = {
        .component = {
            .type = BAGL_RECTANGLE,
            .userid = 0,
            .x = 0,
            .y = 0,
            .width = 128,
            .height = 32,
            .stroke = 0,
            .radius = 0,
            .fill = BAGL_FILL,
            .fgcolor = 0x000000,
            .bgcolor = 0xFFFFFF,
            .icon_id = 0,
            .font_id = 0,
        },
        .text = NULL,
    };

    io_seproxyhal_display_default(&element);
    wait_displayed();
    PRINTF("screen reset\n");
}

static void display_text_line(const char *text, bool bold, bool centered, uint8_t x, uint8_t y) {
    bagl_element_t element = {
        .component = {
            .type = BAGL_LABELINE,
            .userid = 0,
            .x = x,
            .y = y,
            .height = 32,
            .stroke = 0,
            .radius = 0,
            .fill = 0,
            .fgcolor = 0xFFFFFF,
            .bgcolor = 0x000000,
            .icon_id = 0,
        },
        .text = text
    };

    element.component.x = x;
    element.component.y = y;
    element.component.width = 128 - 2 * x;

    if (bold) {
        element.component.font_id = BAGL_FONT_OPEN_SANS_EXTRABOLD_11px;
    } else {
        element.component.font_id = BAGL_FONT_OPEN_SANS_REGULAR_11px;
    }

    if (centered) {
        element.component.font_id |= BAGL_FONT_ALIGNMENT_CENTER;
    }
    io_seproxyhal_display_default(&element);
    wait_displayed();
    PRINTF("text displayed\n");

}

static void display_icon(const bagl_icon_details_t *icon_det, uint8_t x, uint8_t y, uint8_t width, uint8_t height) {
    bagl_component_t component = {
        .type = BAGL_ICON,
        .userid = 0,
        .stroke = 0,
        .radius = 0,
        .fill = 0,
        .fgcolor = 0xFFFFFF,
        .bgcolor = 0x000000,
        .font_id = 0,
        .icon_id = 0,
    };

    component.x = x;
    component.y = y;
    component.width = width;
    component.height = height;

    io_seproxyhal_display_icon(&component, icon_det);
    wait_displayed();
    PRINTF("icon displayed\n");
}

// utility function to compute navigation arrows
static void display_nav_info(nbgl_stepPosition_t pos,
                             uint8_t             nbPages,
                             uint8_t             currentPage,
                             bool                horizontal_nav)
{
    bool right = false;
    bool left = false;

    if (nbPages > 1) {
        if (currentPage > 0) {
            left = true;
        }
        if (currentPage < (nbPages - 1)) {
            right = true;
        }
    }
    if (pos == FIRST_STEP) {
        right = true;
    }
    else if (pos == LAST_STEP) {
        left = true;
    }
    else if (pos == NEITHER_FIRST_NOR_LAST_STEP) {
        right = true;
        left = true;
    }

    if (horizontal_nav) {
        if (left) {
            display_icon(&C_icon_left, 2, 12, 4, 7);
        }
        if (right) {
            display_icon(&C_icon_right, 122, 12, 4, 7);
        }
    } else {
        if (left) {
            display_icon(&C_icon_up, 2, 12, 7, 4);
        }
        if (right) {
            display_icon(&C_icon_down, 122, 12, 7, 4);
        }
    }
}

void layout_buttonCallback(nbgl_buttonEvent_t buttonEvent)
{
    if (!active) {
        return;
    }

    if (ctx_callback != NULL) {
        ctx_callback(buttonEvent);
    }
}

void nbgl_screenHandler(uint32_t intervaleMs)
{
#if 0
    // ensure a screen exists
    if (nbScreensOnStack == 0) {
        return;
    }
    // call ticker callback of top of stack if active and not expired yet (for a non periodic)
    if ((topOfStack->ticker.tickerCallback != NULL) && (topOfStack->ticker.tickerValue != 0)) {
        topOfStack->ticker.tickerValue -= MIN(topOfStack->ticker.tickerValue, intervaleMs);
        if (topOfStack->ticker.tickerValue == 0) {
            // rearm if intervale is not null, and call the registered function
            topOfStack->ticker.tickerValue = topOfStack->ticker.tickerIntervale;
            topOfStack->ticker.tickerCallback();
        }
    }
#endif
}

void nbgl_screenDraw(nbgl_stepPosition_t               pos,
                     nbgl_stepButtonCallback_t         onActionCallback,
                     nbgl_screenTickerConfiguration_t *ticker,
                     const char                       *text,
                     const char                       *subText,
                     const nbgl_icon_details_t        *icon,
                     bool                              centered,
                     bool                              text_bold,
                     bool                              horizontal_nav)
{
    PRINTF("nbgl_screenDraw %d %d %d\n", centered, icon != NULL, subText != NULL);
    if (icon != NULL)
        PRINTF("icon <%p>\n", PIC(icon));
    if (text != NULL)
        PRINTF("text <%s>\n", PIC(text));
    if (subText != NULL)
        PRINTF("subText <%s>\n", PIC(subText));
    active = false;

    ctx_callback = (nbgl_layoutButtonCallback_t) PIC(onActionCallback);
    if (ticker != NULL) {
        memcpy(&ctx_ticker, ticker, sizeof(ctx_ticker));
    } else {
        memset(&ctx_ticker, 0, sizeof(ctx_ticker));
    }

    display_clear();

    uint8_t text_x = 6;
    if (icon != NULL) {
        if (centered) {
            // Consider text is single line and subText is Null
            display_icon(icon, 56, 2, 16, 16);
        } else {
            text_x = 41;
            display_icon(icon, 16, 8, 16, 16);
        }
    }

    if ((icon != NULL) && (centered)) {
        // Consider text is single line and subText is Null
        display_text_line(text, text_bold, centered, text_x, 28);
    } else {
        if (subText != NULL) {
            // Consider text exist and is single line
            display_text_line(text, text_bold, centered, text_x, 12);

            // TODO handle paging
            // For now simply display subText, paging is not handled
            display_text_line(subText, false, centered, text_x, 26);
        } else {
            // TODO handle `\n` and paging? or align in middle if really single line?
            // Consider text exist and is single line
            display_text_line(text, text_bold, centered, text_x, 12);
        }
    }

    display_nav_info(pos, 1, 1, horizontal_nav);
}

void nbgl_refresh(void) {
    active = true;
    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }
}

#endif  // NBGL_STEP
