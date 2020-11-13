#pragma once

#include <stdbool.h>  // bool

/**
 * Structure to display title/text in FLOW.
 */
typedef struct {
    char title[30];
    char text[200];
} strbuf_t;

/**
 * Callback to reuse action with approve/reject in step FLOW.
 */
typedef void (*action_validate_cb)(bool);

/**
 * Function to display public on the device.
 *
 * @brief show public key on the device and ask confirmation to export.
 *
 */
void ui_display_public_key(void);

/**
 * Function to display amount on the device.
 *
 * @brief show amount and recipient and ask confirmation to export.
 *
 */
void ui_display_amount(void);
